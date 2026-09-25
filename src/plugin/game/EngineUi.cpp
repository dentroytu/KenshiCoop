// EngineUi.cpp - the in-game co-op UI plane: the debug marker HUD labels, the
// F2 co-op session panel (native DatapanelGUI), and the persistent status
// overlay. Split out of EngineEntity.cpp (Phase 5e code motion, 2026-07-19) so
// the entity capture/resolve/apply TU stays focused on sync and the UI/render
// surface lives with its public header (EngineUi.h).
//
// Owner state: section-private statics / anon-namespace SEH shims only (marker
// colour + create/update/destroy shims, panel widget pointers, overlay label).
// Must NOT: define g_* engine pointers (EngineInternal.cpp owns them), install
// hooks, or change any log string - "[coop-ui] ..." phrasing is API consumed by
// the harness. The public marker* declarations stay in Engine.h (the Replicator
// uses them for KENSHICOOP_DEBUG_MARKERS); only their definitions moved here.

#include "EngineInternal.h"

// In-game co-op session panel: the native DatapanelGUI window + its interactive
// rows and Win32 key capture. EngineInternal.h already pulls Globals.h (::gui),
// ForgottenGUI.h and MyGUI_Button.h; these add the panel row types, the
// free-function delegate factory, and GetAsyncKeyState/VK_* for the F2 toggle +
// digit entry.
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <mygui/MyGUI_Delegate.h> // MyGUI::newDelegate + CDelegate* (free-fn callbacks)
#include <mygui/MyGUI_DataManager.h>     // accented fonts: find kenshi_fonts.xml
#include <mygui/MyGUI_ResourceManager.h> // accented fonts: register the copies
#include <mygui/MyGUI_XmlDocument.h>     // accented fonts: parse them from memory
#include <mygui/MyGUI_FontManager.h>     // accented fonts: MyGUI's default font
#include <mygui/MyGUI_IFont.h>           // panel line wrapping: glyph advances
#include <windows.h>

#include "../core/SteamId.h" // parseSteamId64 (paste button) + maskSteamId64 (id rows)
#include "../core/UiLang.h" // L(es, en): panel text in the player's language
#include "../core/TextWrap.h" // one panel row per wrapped line
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

namespace coop {
namespace engine {

// ---- Debug marker HUD labels (KENSHICOOP_DEBUG_MARKERS, spike-47 substrate) --
// ForgottenGUI::createScreenLabel + ScreenLabel::setTracking pin a colored text
// label to a character; the engine's own per-frame projection keeps it on the
// body (spike 47 render proof). The Replicator uses these to make join-side
// authority states self-explaining on screen: who is host-driven, who is
// hidden, who is a local-only ghost. C2712 split: the outer fns build the
// std::string/Colour/Vector3 (unwindable), POD-only inner fns hold the SEH.

namespace {

void markerColour(int colorId, MyGUI::Colour* col) {
    switch (colorId) {
    case 0:  *col = MyGUI::Colour(0.30f, 1.00f, 0.30f, 1.0f); break; // driven
    case 1:  *col = MyGUI::Colour(1.00f, 0.25f, 0.25f, 1.0f); break; // hidden
    case 2:  *col = MyGUI::Colour(1.00f, 0.90f, 0.25f, 1.0f); break; // local-only
    default: *col = MyGUI::Colour(0.80f, 0.80f, 0.80f, 1.0f); break;
    }
}

ScreenLabel* markerCreateSeh(ForgottenGUI* g, Character* c,
                             const std::string* text, const MyGUI::Colour* col,
                             const Ogre::Vector3* off) {
    __try {
        ScreenLabel* l = g->createScreenLabel(*text, *col, ScreenLabel::LS_SMALL,
                                              ScreenLabel::RS_STOPPED);
        if (l) {
            l->_NV_setRisingSpeed(ScreenLabel::RS_STOPPED);
            l->_NV_setTracking(c->handle, *off);
        }
        return l;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool markerUpdateSeh(ScreenLabel* l, const std::string* text,
                     const MyGUI::Colour* col) {
    __try {
        l->_NV_setCaption(*text);
        l->_NV_setColor(*col);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Is this label one the GUI still owns? ForgottenGUI keeps the authoritative
// registry of live ScreenLabels (plus deferred add/remove queues), and that
// registry - not our cached pointer - is the only ground truth available: the
// GUI destroys labels on its own schedule and notifies nobody, so a handle held
// across ticks dangles silently. Measured 2026-08-03: the join minted three
// proxies in one burst, debugMark found stale map entries sitting at RECYCLED
// Character* addresses, and setCaption on the previous occupants' dead labels
// faulted twice inside ScreenLabel::setCaption (kenshi+0x6e451e) moments before
// the process died. Same lesson as the world-item proxy hands - never
// dereference a cached engine pointer without re-asking the engine.
//
// Deliberately unlocked. guiScreenLabelsMutex guards these lektors, but taking
// an engine shared_mutex from inside a detour is its own class of hazard, and a
// torn read here is harmless BECAUSE we test for an exact pointer match: garbage
// answers "not present", which costs one discarded marker, whereas a false
// "alive" would need the torn word to equal the very pointer we are asking
// about. The safe direction is the likely one.
bool labelListHas(const lektor<ScreenLabelInterface*>* v, const void* l) {
    __try {
        ScreenLabelInterface* const* p = v->stuff;
        unsigned int n = v->count;
        if (!p || n > 8192u) return false;   // bound a torn/garbage count
        for (unsigned int i = 0; i < n; ++i)
            if ((const void*)p[i] == l) return true;
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool markerDestroySeh(ForgottenGUI* g, ScreenLabel* l) {
    __try {
        g->destroy(l);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

} // namespace

void* markerCreate(Character* c, const char* text, int colorId) {
    if (!c || !text) return 0;
    ForgottenGUI* g = ::gui; // KenshiLib data export (spike 46)
    if (!g) return 0;
    std::string t(text);
    MyGUI::Colour col;
    markerColour(colorId, &col);
    Ogre::Vector3 off(0.0f, 2.2f, 0.0f); // head height (spike 47)
    return markerCreateSeh(g, c, &t, &col, &off);
}

bool markerAlive(void* label) {
    if (!label) return false;
    ForgottenGUI* g = ::gui; // KenshiLib data export (spike 46)
    if (!g) return false;
    // Queued for removal counts as dead: the GUI has already decided, and we
    // would only be racing its next flush.
    if (labelListHas(&g->guiScreenLabelsToRemove, label)) return false;
    // A freshly created label sits in the add queue until the GUI flushes it, so
    // both lists are "alive" or markerCreate's own handle would look dead.
    return labelListHas(&g->guiScreenLabels, label) ||
           labelListHas(&g->guiScreenLabelsToAdd, label);
}

bool markerUpdate(void* label, const char* text, int colorId) {
    if (!label || !text || !markerAlive(label)) return false;
    std::string t(text);
    MyGUI::Colour col;
    markerColour(colorId, &col);
    return markerUpdateSeh((ScreenLabel*)label, &t, &col);
}

void markerDestroy(void* label) {
    if (!label) return;
    ForgottenGUI* g = ::gui; // KenshiLib data export (spike 46)
    if (!g) return;
    // Handing the GUI a label it has already destroyed is the same
    // use-after-free as updating one; prune paths reach here with handles whose
    // Character went away, which is exactly when the GUI has cleaned up too.
    if (!markerAlive(label)) return;
    markerDestroySeh(g, (ScreenLabel*)label);
}

// ---- Accented text (Kenshi's fonts are ASCII-only) ----------------------------
// Every font in Kenshi's data/gui/fonts/kenshi_fonts.xml rasterizes only codes
// 32-126 (plus a few quote marks), so the Spanish panel and banner drew every
// accented letter, n-tilde and inverted mark as a gap (seen in game 2026-09-25).
// We register a copy of each of those fonts named <font>_KC that also covers
// 32-255 - built from the game's own file, so the TTF, size and hinting match -
// and switch only OUR widgets to it; Kenshi's own fonts and widgets are never
// touched (rebuilding Kenshi's fonts in place garbled its whole UI on a window
// resize, 2026-09-25). Kenshi's font-size pass - on a resize, the title screen,
// the font-size option - resizes the copies with its own, but also re-applies
// every widget's font, which sends our EditBox lines back to the ASCII-only
// default font: coopPanelTick re-applies ours to the rows every 500 ms.
// (Buttons and the banner report their font, so that pass keeps our copy.)
// If the copies cannot be made, text is folded to plain ASCII instead (UiLang.h
// foldToAscii): never gaps either way.

namespace {

enum { FONTS_UNTRIED = 0, FONTS_OK = 1, FONTS_FAILED = 2 };
int g_fontState = FONTS_UNTRIED;
std::map<std::string, std::string> g_fontFor; // Kenshi font -> our copy ("" = none)

bool loadAccentFonts(std::string* why) {
    try {
        MyGUI::DataManager*     dm = MyGUI::DataManager::getInstancePtr();
        MyGUI::ResourceManager* rm = MyGUI::ResourceManager::getInstancePtr();
        if (!dm || !rm) { *why = "MyGUI managers not up"; return false; }
        const std::string path = dm->getDataPath("kenshi_fonts.xml");
        if (path.empty()) { *why = "kenshi_fonts.xml not found"; return false; }
        std::ifstream f(path.c_str(), std::ios::binary);
        if (!f) { *why = "cannot open " + path; return false; }
        std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        int renamed = 0, widened = 0;
        std::string xml = coop::accentFontXml(src, &renamed, &widened);
        if (renamed == 0 || widened == 0) { *why = "unexpected kenshi_fonts.xml"; return false; }
        std::istringstream in(xml);
        MyGUI::xml::Document doc;
        if (!doc.open(in) || !doc.getRoot()) { *why = "font XML did not parse"; return false; }
        rm->loadFromXmlNode(doc.getRoot(), path, MyGUI::Version(1, 1, 0));
        return true;
    } catch (...) {
        *why = "MyGUI threw while loading";
        return false;
    }
}

// SEH shell: loadAccentFonts holds C++ objects, so no __try of its own (C2712).
bool loadAccentFontsSeh(std::string* why) {
    __try { return loadAccentFonts(why); }
    __except (EXCEPTION_EXECUTE_HANDLER) { *why = "fault while loading"; return false; }
}

// Once, on the main thread with the GUI up (first panel/banner tick). Marked
// failed BEFORE trying, so a fault is never retried every frame.
void ensureAccentFonts() {
    if (g_fontState != FONTS_UNTRIED || !::gui) return;
    g_fontState = FONTS_FAILED;
    std::string why;
    if (loadAccentFontsSeh(&why)) g_fontState = FONTS_OK;
    if (g_fontState == FONTS_OK)
        coop::logLine("[coop-ui] accented fonts loaded (Kenshi fonts + Latin-1)");
    else
        coop::logErrLine(("[coop-ui] accented fonts unavailable (" + why +
                          "); showing panel text without accents").c_str());
}

// Text for our widgets: as written when the accented fonts are in, else ASCII.
std::string uiText(const std::string& utf8) {
    return g_fontState == FONTS_OK ? utf8 : coop::foldToAscii(utf8);
}

// Our copy of a Kenshi font, if one was registered ("" otherwise).
const std::string& accentFontFor(const std::string& kenshiFont) {
    std::map<std::string, std::string>::iterator it = g_fontFor.find(kenshiFont);
    if (it != g_fontFor.end()) return it->second;
    std::string mine = kenshiFont + coop::accentFontSuffix();
    MyGUI::ResourceManager* rm = MyGUI::ResourceManager::getInstancePtr();
    bool ok = false;
    try { ok = rm && rm->isExist(mine); } catch (...) { ok = false; }
    coop::logLine(("[coop-ui] font '" + kenshiFont + "' -> " +
                   (ok ? "'" + mine + "'" : std::string("no accented copy"))).c_str());
    return g_fontFor[kenshiFont] = ok ? mine : std::string();
}

// C2712 split: the string work stays out here, the widget calls in POD frames.
const std::string* widgetFontSeh(MyGUI::TextBox* w) {
    __try { return &w->getFontName(); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
void setWidgetFontSeh(MyGUI::TextBox* w, const std::string* font) {
    __try { w->setFontName(*font); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
// An empty font name means MyGUI's default font. The panel's text lines are
// EditBoxes, and an EditBox always reports an empty name (it keeps its font in
// its own client text, which getFontName does not read) while setFontName does
// work on it - so an empty name is resolved to the default font's copy. Only
// the buttons report their font (Kenshi_StandardFont_Medium). Seen 2026-09-25.
std::string defaultFontName() {
    try {
        MyGUI::FontManager* fm = MyGUI::FontManager::getInstancePtr();
        return fm ? fm->getDefaultFont() : std::string();
    } catch (...) { return std::string(); }
}

void useAccentFont(MyGUI::TextBox* w) {
    if (!w || g_fontState != FONTS_OK) return;
    const std::string* cur = widgetFontSeh(w);
    if (!cur) return;
    std::string font = cur->empty() ? defaultFontName() : *cur; // copy: *cur changes below
    if (font.empty()) return;
    const std::string suffix = coop::accentFontSuffix();
    if (font.size() > suffix.size() &&
        font.compare(font.size() - suffix.size(), suffix.size(), suffix) == 0) return;
    std::string mine = accentFontFor(font);
    if (!mine.empty()) setWidgetFontSeh(w, &mine);
}

} // namespace

// ---- In-game co-op session panel (config-driven, spike-50 DatapanelGUI stack) -
// A native DatapanelGUI window toggled with F2. The player picks role + transport
// (toggle BUTTONS - the only DatapanelGUI control with a callable RVA callback;
// MyGUI comboboxes/editboxes have no reachable getters and never receive keyboard
// focus during gameplay) and connects/leaves via a bound checkbox. The friend code
// (peer SteamID) + UDP endpoint come from coop_config.json and are shown READ-ONLY;
// a "Copy my Steam ID" button puts the player's own id on the clipboard to share.
// The GUI layer is session-agnostic: live status arrives via *st; the user's
// actions leave via the onConnect/onDisconnect callbacks (the plugin root owns the
// net/session/config wiring).
//
// SEH discipline (spike 47/48): the mutation calls take std::string by const-ref
// or PODs, so they all sit inside one __try, provided NO std::string temporary is
// constructed in that frame. The one exception is createDatapanel's BY-VALUE
// std::string 'layer' arg (an unwindable temporary => C2712), so the window is
// created in the outer, non-SEH function; ::gui is verified non-null first and the
// createScreenLabel/createFloatingLabel factory family is render-proven (46-48).

namespace {

// Is one of this process's windows in the foreground? GetAsyncKeyState reads the
// keyboard system-wide, so without this check F2 pressed in a browser, a chat app
// or the other Kenshi of a two-client test toggled this panel too.
bool gameHasFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

// Write a UTF-8/ANSI string to the Windows clipboard (CF_TEXT). Mirror of the
// paste-read: OpenClipboard -> EmptyClipboard -> GlobalAlloc+copy -> SetClipboardData
// -> CloseClipboard. Used by the "Copy my Steam ID" button. Win32 only (no MyGUI).
bool clipboardSetText(const char* text) {
    if (!text) return false;
    size_t n = strlen(text);
    if (!OpenClipboard(0)) return false;
    bool ok = false;
    if (EmptyClipboard()) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n + 1);
        if (h) {
            char* dst = (char*)GlobalLock(h);
            if (dst) {
                memcpy(dst, text, n);
                dst[n] = '\0';
                GlobalUnlock(h);
                if (SetClipboardData(CF_TEXT, h)) ok = true; // clipboard now owns h
            }
            if (!ok) GlobalFree(h); // ownership not transferred on failure
        }
    }
    CloseClipboard();
    return ok;
}

// Read text from the Windows clipboard into out. Prefers CF_UNICODETEXT (what the
// Steam overlay / browsers usually publish) and falls back to CF_TEXT, converting
// either to a narrow std::string (the SteamID parse keeps only ASCII digits, so a
// lossy WideCharToMultiByte is fine here). Used by the "Paste friend's Steam ID"
// button. Win32 only (no MyGUI). Returns true iff some text was retrieved.
bool clipboardGetText(std::string& out) {
    if (!OpenClipboard(0)) return false;
    bool ok = false;
    HANDLE hw = GetClipboardData(CF_UNICODETEXT);
    if (hw) {
        const wchar_t* src = (const wchar_t*)GlobalLock(hw);
        if (src) {
            int need = WideCharToMultiByte(CP_UTF8, 0, src, -1, 0, 0, 0, 0);
            if (need > 0) {
                std::string tmp((size_t)need, '\0');
                if (WideCharToMultiByte(CP_UTF8, 0, src, -1, &tmp[0], need, 0, 0) > 0) {
                    if (!tmp.empty() && tmp[tmp.size() - 1] == '\0') tmp.resize(tmp.size() - 1);
                    out = tmp;
                    ok = true;
                }
            }
            GlobalUnlock(hw);
        }
    }
    if (!ok) {
        HANDLE ha = GetClipboardData(CF_TEXT);
        if (ha) {
            const char* src = (const char*)GlobalLock(ha);
            if (src) { out = src; ok = true; GlobalUnlock(ha); }
        }
    }
    CloseClipboard();
    return ok;
}

// Panel views. MAIN is what a player needs (status, "Invite a Steam friend",
// how to accept an invite, Disconnect); PICK lists online friends to invite;
// ADVANCED keeps the original manual controls (role / transport / connection
// toggles and the Steam ID copy-paste) for LAN/UDP or when invites fail.
enum { VIEW_MAIN = 0, VIEW_PICK = 1, VIEW_ADVANCED = 2 };

struct CoopPanelUi {
    DatapanelGUI* panel;
    bool          open, built;
    bool          hostFlag;      // true = HOST role armed
    bool          steamFlag;     // true = Steam transport armed (else UDP)
    bool          connectedFlag; // desired connection state (Online/Offline toggle)
    bool          lastConnected; // last observed st->running (external-change sync)
    bool          lastChkVal;    // last toggle value (connect/disconnect edge)
    bool          needsRebuild;
    bool          f2Down;        // F2 held last tick (rising-edge toggle)
    int           view;          // VIEW_*
    int           linePx;        // text width of a built row, for wrapping (0 = unknown)
    int           rowPx;         // height of a built row (0 = unknown)
    int           fontPx;        // the row font's own height (0 = unknown)
    std::string   lastSig;       // rows shown by the last build (refresh gate)
    CoopPanelUi()
        : panel(0), open(false), built(false), hostFlag(true), steamFlag(true),
          connectedFlag(false), lastConnected(false), lastChkVal(false),
          needsRebuild(false), f2Down(false), view(VIEW_MAIN), linePx(0), rowPx(0), fontPx(0) {}
};

CoopPanelUi             g_panel;
std::string             g_selfIdStr;   // self SteamID as digits (set each tick; "" = none)
DWORD                   g_uiThread = 0; // main thread, from coopPanelTick (coopUiShutdown guard)

// Friend's SteamID pasted in-panel this session (0 = none). Per-session by
// design: it lives only in memory, so relaunching Kenshi clears it and the
// friend's id is re-pasted (nothing is written to disk). Passed to onConnect,
// where it overrides the (usually empty) config steamPeer.
unsigned long long      g_pastedPeer   = 0;
bool                    g_pasteFailed  = false; // last paste wasn't a valid Steam ID

// Steam invite picker: g_pickIds[i] is the friend behind picker button i. The
// callbacks are refreshed at the top of every coopPanelTick so the free-fn
// button handlers can reach them.
const int               MAX_PICK       = 6;
unsigned long long      g_pickIds[MAX_PICK]  = {0};
CoopInviteBeginFn       g_onInviteBegin  = 0;
CoopInviteFriendFn      g_onInviteFriend = 0;
std::string             g_peerModsCfg;  // friend's mod list (mods.cfg form), set each tick

// Button callbacks (free functions - MyGUI::newDelegate wraps them without any
// raw-MyGUI link). A press changes panel state and requests a rebuild so the
// rows reflect it on the next tick.
void onRoleBtn(DataPanelLine*) {
    g_panel.hostFlag = !g_panel.hostFlag;
    g_panel.needsRebuild = true;
    coop::logLine(g_panel.hostFlag ? "[coop-ui] role -> Host" : "[coop-ui] role -> Join");
}
void onTransBtn(DataPanelLine*) {
    g_panel.steamFlag = !g_panel.steamFlag;
    g_panel.needsRebuild = true;
    coop::logLine(g_panel.steamFlag ? "[coop-ui] transport -> Steam" : "[coop-ui] transport -> UDP");
}
// Online/Offline toggle: flip the desired connection state. The connect/disconnect
// edge (connectedFlag vs lastChkVal) is handled in coopPanelTick.
void onConnBtn(DataPanelLine*) {
    g_panel.connectedFlag = !g_panel.connectedFlag;
    g_panel.needsRebuild = true;
    coop::logLine(g_panel.connectedFlag ? "[coop-ui] connection -> ONLINE"
                                        : "[coop-ui] connection -> OFFLINE");
}
// Main-view Disconnect / Cancel: same edge as the toggle, forced OFFLINE.
void onDisconnectBtn(DataPanelLine*) {
    g_panel.connectedFlag = false;
    g_panel.needsRebuild = true;
    coop::logLine("[coop-ui] connection -> OFFLINE");
}
// Copy the player's own SteamID to the clipboard so they can paste it to a friend
// (who pastes it into their panel via "Paste friend's Steam ID").
void onCopyIdBtn(DataPanelLine*) {
    if (g_selfIdStr.empty()) {
        coop::logLine("[coop-ui] copy Steam ID: none (Steam not running)");
        return;
    }
    bool ok = clipboardSetText(g_selfIdStr.c_str());
    char b[64];
    _snprintf(b, sizeof(b) - 1, "[coop-ui] copied Steam ID to clipboard: %s",
              ok ? "ok" : "FAILED");
    b[sizeof(b) - 1] = '\0';
    coop::logLine(b);
}
// Paste the friend's SteamID from the clipboard: read text, extract + validate a
// SteamID64, and store it as the session peer (used on the next Connect). No
// typing, no config edit. Rejects arbitrary clipboard junk (g_pasteFailed drives
// the peer-row hint).
void onPasteIdBtn(DataPanelLine*) {
    std::string clip;
    unsigned long long id = 0;
    if (clipboardGetText(clip) && coop::parseSteamId64(clip, id)) {
        g_pastedPeer  = id;
        g_pasteFailed = false;
        char b[64];
        _snprintf(b, sizeof(b) - 1, "[coop-ui] paste friend id=%llu ok=1", id);
        b[sizeof(b) - 1] = '\0';
        coop::logLine(b);
    } else {
        g_pasteFailed = true;
        coop::logLine("[coop-ui] paste friend id=0 ok=0 (clipboard not a Steam ID)");
    }
    g_panel.needsRebuild = true;
}
// Copy the friend's active mods (load order, mods.cfg form) to the clipboard so
// the player can compare or rebuild their own list in the launcher.
void onCopyModsBtn(DataPanelLine*) {
    if (g_peerModsCfg.empty()) return;
    bool ok = clipboardSetText(g_peerModsCfg.c_str());
    coop::logLine(ok ? "[coop-ui] copied friend's mod list to clipboard: ok"
                     : "[coop-ui] copied friend's mod list to clipboard: FAILED");
}
// "Invite a Steam friend": switch to the picker, which creates the friends-only
// lobby (async) and fills the friend list.
void onInviteBtn(DataPanelLine*) {
    g_panel.view = VIEW_PICK;
    g_panel.needsRebuild = true;
    coop::logLine("[coop-ui] invite picker opened");
    if (g_onInviteBegin) g_onInviteBegin();
}
// Back to the main view. Leaving the picker keeps the lobby, so an invite already
// sent can still be accepted.
void onBackBtn(DataPanelLine*) {
    if (g_panel.view == VIEW_PICK) coop::logLine("[coop-ui] invite picker closed");
    g_panel.view = VIEW_MAIN;
    g_panel.needsRebuild = true;
}
void onAdvancedBtn(DataPanelLine*) {
    g_panel.view = VIEW_ADVANCED;
    g_panel.needsRebuild = true;
    coop::logLine("[coop-ui] advanced options opened");
}
// One handler per picker row (MyGUI delegates carry no row context).
template <int I>
void onPickBtn(DataPanelLine*) {
    unsigned long long id = g_pickIds[I];
    if (id == 0) return;
    char b[64];
    _snprintf(b, sizeof(b) - 1, "[coop-ui] invite friend row=%d", I);
    b[sizeof(b) - 1] = '\0';
    coop::logLine(b);
    if (g_onInviteFriend) g_onInviteFriend(id);
    g_panel.needsRebuild = true;
}

// ---- Row model -------------------------------------------------------------------
// Each view is a list of rows (text line, button or spacer). The rows are rebuilt
// every tick as strings and only pushed into the DatapanelGUI when their
// signature changes. Line colour carries the state (red / amber / green).
enum RowKind { ROW_LINE, ROW_BUTTON, ROW_SPACE };
enum RowAct {
    ACT_NONE, ACT_ROLE, ACT_TRANS, ACT_CONN, ACT_COPYID, ACT_PASTEID, ACT_COPYMODS,
    ACT_INVITE, ACT_BACK, ACT_ADVANCED, ACT_DISCONNECT, ACT_PICK0
};
enum RowCol { COL_WHITE, COL_GREY, COL_RED, COL_AMBER, COL_GREEN };

struct Row {
    int         kind;
    std::string text;
    int         act;
    int         col;
    Row() : kind(ROW_SPACE), act(ACT_NONE), col(COL_WHITE) {} // VS2010 vector::resize needs it
    Row(int k, const std::string& t, int a, int c) : kind(k), text(t), act(a), col(c) {}
};
void addLine(std::vector<Row>& r, const std::string& t, int col) { r.push_back(Row(ROW_LINE, t, ACT_NONE, col)); }
void addButton(std::vector<Row>& r, const std::string& t, int act) { r.push_back(Row(ROW_BUTTON, t, act, COL_WHITE)); }
void addSpace(std::vector<Row>& r) { r.push_back(Row(ROW_SPACE, std::string(), ACT_NONE, COL_WHITE)); }

const int              MAX_ROWS = 32; // wrapped lines take a row each
DataPanelLine*         g_rowLine[MAX_ROWS];
DataPanelLine_Button*  g_rowBtn[MAX_ROWS];

// POD view of the rows so the build SEH frame constructs no std::string.
struct RowPod { int kind; const std::string* key; const std::string* text; };

void panelBuildSeh(DatapanelGUI* p, const std::string* title, const RowPod* rows, int n,
                   const std::string* empty) {
    __try {
        p->_NV_clear();
        for (int i = 0; i < MAX_ROWS; ++i) { g_rowLine[i] = 0; g_rowBtn[i] = 0; }
        p->setCaption(*title);
        for (int i = 0; i < n; ++i) {
            if (rows[i].kind == ROW_SPACE)
                p->addSpace(0, 0.35f);
            else if (rows[i].kind == ROW_BUTTON)
                g_rowBtn[i] = p->setLineButton(*rows[i].key, *rows[i].text, 0);
            else
                g_rowLine[i] = p->setLine(*rows[i].key, *rows[i].text, *empty, 0, false, true);
        }
        p->_NV_update();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Colour a line's TextBoxes. Runs AFTER panelBuildSeh's _NV_update (the w1/w2
// widgets exist by then). MyGUI::Colour is a trivial 4-float struct (no
// destructor), so it may live in the SEH frame.
void lineColourSeh(DataPanelLine* line, float r, float g, float b) {
    if (!line) return;
    __try {
        MyGUI::Colour c(r, g, b, 1.0f);
        if (line->w1) line->w1->setTextColour(c);
        if (line->w2) line->w2->setTextColour(c);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
// The text widgets of one built row: w1/w2 of its line, plus a button row's
// button (a MyGUI::Button is a TextBox). Unset entries stay 0.
void rowTextWidgetsSeh(DataPanelLine* line, DataPanelLine_Button* btn, MyGUI::TextBox* out[3]) {
    __try {
        DataPanelLine* l = line ? line : btn;
        if (l) { out[0] = l->w1; out[1] = l->w2; }
        if (btn) out[2] = btn->button;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
void setWidgetFontHeightSeh(MyGUI::TextBox* w, int px) {
    if (!w) return;
    __try { w->setFontHeight(px); } // virtual (EditBox forwards it to its text)
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
// Put the accented copies on every text widget of the built panel (after a
// build, and again every 500 ms: see "Accented text" on Kenshi's font pass).
// In a small window a row is shorter than the font (11 px rows, 14 px font at
// 944x700), which cut every descender ("amigo" read "amieo"): the text lines
// then draw at the row's height. Buttons centre their text and are left alone.
// Setting a font resets its height, so both are re-applied together.
void accentPanelRows() {
    const bool shrink = g_panel.rowPx > 0 && g_panel.fontPx > g_panel.rowPx;
    for (int i = 0; i < MAX_ROWS; ++i) {
        if (!g_rowLine[i] && !g_rowBtn[i]) continue;
        MyGUI::TextBox* w[3] = { 0, 0, 0 };
        rowTextWidgetsSeh(g_rowLine[i], g_rowBtn[i], w);
        for (int k = 0; k < 3; ++k) useAccentFont(w[k]);
        if (shrink && g_rowLine[i]) {
            setWidgetFontHeightSeh(w[0], g_panel.rowPx);
            setWidgetFontHeightSeh(w[1], g_panel.rowPx);
        }
    }
}
// How much narrower the lines draw than the font's own metrics (see above).
float panelTextScale() {
    return (g_panel.rowPx > 0 && g_panel.fontPx > g_panel.rowPx)
         ? (float)g_panel.rowPx / (float)g_panel.fontPx : 1.0f;
}
// Line wrapping (core/TextWrap.h), measured with the font the rows draw with:
// the default font's accented copy once it is in, else the default font.
float glyphAdvanceSeh(MyGUI::IFont* f, unsigned int cp) {
    __try {
        MyGUI::GlyphInfo* g = f->getGlyphInfo((MyGUI::Char)cp); // virtual
        if (g) return g->bearingX + g->advance;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return 7.0f;
}
float panelGlyphAdvance(unsigned int cp, void* font) {
    return glyphAdvanceSeh(static_cast<MyGUI::IFont*>(font), cp);
}
MyGUI::IFont* panelTextFont() {
    std::string name = defaultFontName();
    if (name.empty()) return 0;
    if (g_fontState == FONTS_OK) {
        const std::string& mine = accentFontFor(name);
        if (!mine.empty()) name = mine;
    }
    try {
        MyGUI::FontManager* fm = MyGUI::FontManager::getInstancePtr();
        return fm ? fm->getByName(name) : 0;
    } catch (...) { return 0; }
}
// Width of a built line's text widget: the width its EditBox wraps at.
int lineTextWidthSeh(DataPanelLine* line) {
    if (!line) return 0;
    __try { return line->w1 ? line->w1->getWidth() : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
int lineTextHeightSeh(DataPanelLine* line) {
    if (!line) return 0;
    __try { return line->w1 ? line->w1->getHeight() : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
int fontHeightSeh(MyGUI::IFont* f) {
    __try { return f->getDefaultHeight(); } // virtual
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
// Re-read a built line's size and the row font's height; a change (first
// build, a window resize, Kenshi's font-size pass) asks for a rebuild so the
// rows are wrapped - and their text sized - for the new geometry.
void refreshLineWidth() {
    for (int i = 0; i < MAX_ROWS; ++i) {
        if (!g_rowLine[i]) continue;
        const int px = lineTextWidthSeh(g_rowLine[i]);
        if (px <= 0) continue;
        const int rowPx = lineTextHeightSeh(g_rowLine[i]);
        MyGUI::IFont* f = panelTextFont();
        const int fontPx = f ? fontHeightSeh(f) : 0;
        if (px != g_panel.linePx || rowPx != g_panel.rowPx || fontPx != g_panel.fontPx) {
            g_panel.linePx = px;
            g_panel.rowPx  = rowPx;
            g_panel.fontPx = fontPx;
            g_panel.needsRebuild = true;
            char b[160];
            _snprintf(b, sizeof(b) - 1,
                      "[coop-ui] panel rows %dx%d px, font %d px (text scale %.2f)",
                      px, rowPx, fontPx, panelTextScale());
            b[sizeof(b) - 1] = '\0';
            coop::logLine(b);
        }
        return;
    }
}

void colourRow(DataPanelLine* line, int col) {
    switch (col) {
    case COL_GREY:  lineColourSeh(line, 0.72f, 0.72f, 0.72f); break;
    case COL_RED:   lineColourSeh(line, 1.00f, 0.42f, 0.36f); break;
    case COL_AMBER: lineColourSeh(line, 1.00f, 0.82f, 0.20f); break;
    case COL_GREEN: lineColourSeh(line, 0.45f, 0.95f, 0.45f); break;
    default:        lineColourSeh(line, 1.00f, 1.00f, 1.00f); break;
    }
}
void bindButton(DataPanelLine_Button* b, int act) {
    if (!b) return;
    switch (act) {
    case ACT_ROLE:       b->callback = MyGUI::newDelegate(&onRoleBtn); break;
    case ACT_TRANS:      b->callback = MyGUI::newDelegate(&onTransBtn); break;
    case ACT_CONN:       b->callback = MyGUI::newDelegate(&onConnBtn); break;
    case ACT_COPYID:     b->callback = MyGUI::newDelegate(&onCopyIdBtn); break;
    case ACT_PASTEID:    b->callback = MyGUI::newDelegate(&onPasteIdBtn); break;
    case ACT_COPYMODS:   b->callback = MyGUI::newDelegate(&onCopyModsBtn); break;
    case ACT_INVITE:     b->callback = MyGUI::newDelegate(&onInviteBtn); break;
    case ACT_BACK:       b->callback = MyGUI::newDelegate(&onBackBtn); break;
    case ACT_ADVANCED:   b->callback = MyGUI::newDelegate(&onAdvancedBtn); break;
    case ACT_DISCONNECT: b->callback = MyGUI::newDelegate(&onDisconnectBtn); break;
    case ACT_PICK0 + 0:  b->callback = MyGUI::newDelegate(&onPickBtn<0>); break;
    case ACT_PICK0 + 1:  b->callback = MyGUI::newDelegate(&onPickBtn<1>); break;
    case ACT_PICK0 + 2:  b->callback = MyGUI::newDelegate(&onPickBtn<2>); break;
    case ACT_PICK0 + 3:  b->callback = MyGUI::newDelegate(&onPickBtn<3>); break;
    case ACT_PICK0 + 4:  b->callback = MyGUI::newDelegate(&onPickBtn<4>); break;
    case ACT_PICK0 + 5:  b->callback = MyGUI::newDelegate(&onPickBtn<5>); break;
    default: break;
    }
}

// The invite layer's status, worded for the player (codes: steaminvite::ST_*).
std::string inviteStatusText(int code, const char* arg) {
    std::string who = (arg && arg[0]) ? std::string(arg) : std::string(L("tu amigo", "your friend"));
    switch (code) {
    case 1: return L("Elige a qu\xC3\xA9 amigo invitar:", "Pick a friend to invite:");
    case 2: return std::string(L("Invitaci\xC3\xB3n enviada a ", "Invite sent to ")) + who +
                   L(". Esperando a que acepte...", ". Waiting for them to accept...");
    case 9: return L("Invitaci\xC3\xB3n enviada. Esperando a que acepte...", "Invite sent. Waiting for them to accept...");
    case 3: return L("No se pudo crear la sala de Steam. Vuelve a intentarlo.",
                     "Could not create the Steam lobby. Try again.");
    case 4: return L("Tu amigo ha aceptado. Conectando...", "Your friend accepted. Connecting...");
    case 5: return L("Entrando en la partida de tu amigo...", "Joining your friend's game...");
    case 6: return L("Conectando con tu amigo...", "Connecting to your friend...");
    case 7: return L("Tu amigo tiene otra versi\xC3\xB3n de KenshiCoop. Instalad la misma los dos.",
                     "Your friend has a different KenshiCoop version. Install the same one.");
    case 8: return L("Steam no est\xC3\xA1 disponible. Abre Kenshi desde Steam.",
                     "Steam is not available. Start Kenshi from Steam.");
    default: return std::string();
    }
}

// Arm a freshly-minted panel: register it for ForgottenGUI's per-frame refresh
// AND make it visible. createDatapanel returns a built-but-hidden window; without
// this pair the F2 toggle logs open/close yet nothing ever draws (the render bug
// in the reconstruction). PODs only, so the whole thing sits in one SEH frame.
bool uiPanelArmSeh(ForgottenGUI* g, DatapanelGUI* p) {
    if (!g || !p) return false;
    __try {
        g->addDatapanelToUpdateList(p);
        p->_NV_show(true);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void panelDestroySeh(ForgottenGUI* g, DatapanelGUI* p) {
    if (!g || !p) return;
    // Pull it off the refresh list BEFORE destroying so ForgottenGUI never
    // dereferences the freed panel on the next frame.
    __try {
        g->removeDatapanelFromUpdateList(p);
        g->destroy(p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void clearRowPointers() {
    for (int i = 0; i < MAX_ROWS; ++i) { g_rowLine[i] = 0; g_rowBtn[i] = 0; }
}

} // namespace

void coopPanelTick(const CoopPanelState* st, CoopConnectFn onConnect,
                   CoopDisconnectFn onDisconnect, CoopInviteBeginFn onInviteBegin,
                   CoopInviteFriendFn onInviteFriend) {
    if (!st) return;
    g_uiThread       = GetCurrentThreadId();
    g_onInviteBegin  = onInviteBegin;
    g_onInviteFriend = onInviteFriend;
    ForgottenGUI* g = ::gui; // KenshiLib data export (spike 46)
    { static void* s_last = (void*)-1;
      if ((void*)g != s_last) { s_last = (void*)g;
          char b[64]; _snprintf(b, sizeof(b) - 1, "[coop-ui] gui ptr=%p", (void*)g);
          b[sizeof(b) - 1] = '\0'; coop::logLine(b); } }
    if (!g) return;
    ensureAccentFonts();

    // Cache the self id as a string for the Copy button (used by onCopyIdBtn).
    if (st->selfSteamId) {
        char b[32];
        _snprintf(b, sizeof(b) - 1, "%llu", (unsigned long long)st->selfSteamId);
        b[sizeof(b) - 1] = '\0';
        g_selfIdStr = b;
    } else {
        g_selfIdStr.clear();
    }
    g_peerModsCfg = st->peerModsCfg ? std::string(st->peerModsCfg) : std::string();

    // F2 rising edge toggles the panel open/closed (it always opens on MAIN).
    // Only while Kenshi has focus: the key state itself is system-wide.
    bool f2 = gameHasFocus() && (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    if (f2 && !g_panel.f2Down) {
        if (!g_panel.open) {
            g_panel.hostFlag      = st->isHost;
            g_panel.steamFlag     = (st->transportSel == 0);
            g_panel.connectedFlag = st->running;
            g_panel.lastConnected = st->running;
            g_panel.lastChkVal    = st->running;
            g_panel.view          = VIEW_MAIN;
            g_panel.open = true;
            g_panel.needsRebuild = true;
            coop::logLine("[coop-ui] panel opened");
        } else {
            panelDestroySeh(g, g_panel.panel);
            g_panel.panel = 0; g_panel.built = false;
            clearRowPointers();
            g_panel.open = false;
            coop::logLine("[coop-ui] panel closed");
        }
    }
    g_panel.f2Down = f2;

    if (!g_panel.open) return;

    // Keep the Online/Offline state honest when the session changes underneath us
    // (an invite connected us, a failed connect stopped, etc.).
    if (st->running != g_panel.lastConnected) {
        g_panel.lastConnected = st->running;
        g_panel.connectedFlag = st->running;
        g_panel.lastChkVal    = st->running;
        // A Steam invite connects without the Role button (the inviter hosts,
        // the invitee joins), so show the role that actually started.
        if (st->running) g_panel.hostFlag = st->isHost;
        g_panel.needsRebuild = true;
    }

    // ---- What the player sees ----------------------------------------------------
    std::string status;
    int statusCol;
    std::string waitLine; // what to do next while connected at the main menu
    if (st->running && st->waitNote == 1) {
        status = L("Tu amigo ya est\xC3\xA1 conectado", "Your friend is connected");
        statusCol = COL_GREEN;
        waitLine = L("Carga una partida (o empieza una nueva) y entrar\xC3\xA1 contigo.",
                     "Load a game (or start a new one) and your friend joins you.");
    } else if (st->running && st->waitNote == 2) {
        status = L("Conectado con tu amigo", "Connected to your friend");
        statusCol = COL_GREEN;
        waitLine = L("Esperando a que tu amigo cargue su partida...",
                     "Waiting for your friend to load their game...");
    } else if (st->running && st->peerPresent) {
        status = st->isHost ? L("Conectado: tu amigo est\xC3\xA1 en tu partida", "Connected: your friend is in your game")
                            : L("Conectado a la partida de tu amigo", "Connected to your friend's game");
        statusCol = COL_GREEN;
    } else if (st->running && st->refuseFinal) {
        status = L("No se pudo conectar", "Could not connect");
        statusCol = COL_RED;
    } else if (st->running) {
        status = st->isHost ? L("Esperando a tu amigo...", "Waiting for your friend...")
                            : L("Conectando con tu amigo...", "Connecting to your friend...");
        statusCol = COL_AMBER;
    } else {
        status = L("Sin conectar", "Not connected");
        statusCol = COL_RED;
    }
    std::string transfer = st->transferDetail ? std::string(st->transferDetail) : std::string();
    std::string inviteText = inviteStatusText(st->inviteCode, st->inviteArg);
    bool canInvite = st->inviteReady && g_panel.steamFlag && !st->peerPresent &&
                     st->waitNote == 0 && (!st->running || st->isHost);
    if (g_panel.view == VIEW_PICK && !canInvite) g_panel.view = VIEW_MAIN;

    std::vector<Row> rows;
    unsigned long long pickIds[MAX_PICK] = {0};
    if (g_panel.view == VIEW_PICK) {
        addLine(rows, status, statusCol);
        addLine(rows, inviteText.empty() ? std::string(L("Buscando amigos...", "Looking for friends...")) : inviteText,
                COL_WHITE);
        addSpace(rows);
        int pickN = 0;
        for (int i = 0; i < st->friendN && pickN < MAX_PICK; ++i) {
            const CoopFriendRow& f = st->friends[i];
            if (f.state == 0 || f.id == 0) continue; // offline friends can't accept
            pickIds[pickN] = f.id;
            addButton(rows, std::string(L("Invitar a ", "Invite ")) + (f.name ? f.name : "?") +
                            (f.state == 2 ? L("    (jugando a Kenshi)", "    (playing Kenshi)")
                                          : L("    (conectado a Steam)", "    (online)")),
                      ACT_PICK0 + pickN);
            ++pickN;
        }
        if (pickN == 0)
            addLine(rows, L("Ning\xC3\xBAn amigo conectado a Steam ahora mismo.", "No friends online on Steam right now."),
                    COL_GREY);
        addSpace(rows);
        addButton(rows, L("Volver", "Back"), ACT_BACK);
    } else if (g_panel.view == VIEW_ADVANCED) {
        addLine(rows, status + (g_panel.steamFlag ? "  (Steam)" : "  (UDP)"), statusCol);
        // The manual (Steam ID / UDP) connection is made from this view, so the
        // reason has to show here too, not only on the main view.
        if (st->refuseNotice) {
            addLine(rows, st->refuseNotice, st->refuseLevel >= 2 ? COL_RED : COL_AMBER);
            if (st->refuseHint) addLine(rows, st->refuseHint, COL_GREY);
        }
        addSpace(rows);
        addButton(rows, std::string(L("Rol: ", "Role: ")) +
                        (g_panel.hostFlag ? L("ANFITRI\xC3\x93N", "HOST") : L("UNIRSE", "JOIN")) +
                        L("    (cambiar)", "    (switch)"), ACT_ROLE);
        addButton(rows, std::string(L("Conexi\xC3\xB3n por: ", "Transport: ")) +
                        (g_panel.steamFlag ? "STEAM" : "UDP") + L("    (cambiar)", "    (switch)"), ACT_TRANS);
        // After a final refusal the link is still up (idle, not retrying), so
        // "ONLINE" would contradict "could not connect" right above it.
        addButton(rows, std::string(L("Estado: ", "Connection: ")) +
                        (!g_panel.connectedFlag ? L("DESCONECTADO", "OFFLINE")
                         : st->refuseFinal      ? L("RECHAZADO", "REFUSED")
                                                : L("CONECTADO", "ONLINE")) +
                        L("    (cambiar)", "    (switch)"), ACT_CONN);
        if (!g_panel.steamFlag)
            addLine(rows, L("UDP: la IP y el puerto est\xC3\xA1n en mods\\KenshiCoop\\coop_config.json",
                            "UDP: IP and port are in mods\\KenshiCoop\\coop_config.json"), COL_GREY);
        addSpace(rows);
        // Friend's SteamID: prefer the value pasted in-panel this session; fall
        // back to the config (steamPeer). Only the last 4 digits are shown.
        unsigned long long peerShown = g_pastedPeer ? g_pastedPeer : (unsigned long long)st->peerSteamId;
        if (peerShown != 0)
            addLine(rows, std::string(L("Steam ID de tu amigo: ", "Friend's Steam ID: ")) +
                          coop::maskSteamId64(peerShown), COL_WHITE);
        else if (g_pasteFailed)
            addLine(rows, L("Lo copiado no era un Steam ID: copia el de tu amigo y reintenta.",
                            "The clipboard was not a Steam ID: copy your friend's and retry."), COL_AMBER);
        else
            addLine(rows, L("Steam ID de tu amigo: (p\xC3\xA9galo con el bot\xC3\xB3n)", "Friend's Steam ID: (paste it below)"),
                    COL_GREY);
        addButton(rows, L("Pegar el Steam ID de tu amigo", "Paste friend's Steam ID"), ACT_PASTEID);
        addLine(rows, std::string(L("Tu Steam ID: ", "Your Steam ID: ")) +
                      (st->selfSteamId ? coop::maskSteamId64((unsigned long long)st->selfSteamId)
                                       : std::string(L("(Steam no est\xC3\xA1 abierto)", "(Steam not running)"))),
                COL_WHITE);
        addButton(rows, L("Copiar mi Steam ID", "Copy my Steam ID"), ACT_COPYID);
        addSpace(rows);
        addButton(rows, L("Volver", "Back"), ACT_BACK);
    } else {
        addLine(rows, status, statusCol);
        if (st->refuseNotice) {
            addLine(rows, st->refuseNotice, st->refuseLevel >= 2 ? COL_RED : COL_AMBER);
            if (st->refuseHint) addLine(rows, st->refuseHint, COL_GREY);
        }
        if (!waitLine.empty()) addLine(rows, waitLine, COL_AMBER);
        if (!transfer.empty()) addLine(rows, transfer, COL_AMBER);
        if (!inviteText.empty() && st->inviteCode != 1 && !st->refuseFinal)
            addLine(rows, inviteText, COL_WHITE);
        if (st->modsLine) {
            addLine(rows, st->modsLine, st->modsWarn ? COL_AMBER : COL_GREEN);
            if (st->modsWarn && !g_peerModsCfg.empty())
                addButton(rows, L("Copiar la lista de mods de tu amigo", "Copy friend's mod list"), ACT_COPYMODS);
        }
        addSpace(rows);
        if (canInvite) {
            addButton(rows, L("Invitar a un amigo de Steam", "Invite a Steam friend"), ACT_INVITE);
            addLine(rows, L("Tu amigo acepta la invitaci\xC3\xB3n en Steam y entr\xC3\xA1is los dos.",
                            "Your friend accepts the Steam invite and you both connect."), COL_GREY);
        } else if (!st->inviteReady && !st->running) {
            addLine(rows, L("Steam no est\xC3\xA1 disponible: abre Kenshi desde Steam o usa Opciones avanzadas.",
                            "Steam is not available: start Kenshi from Steam or use Advanced options."), COL_AMBER);
        }
        if (!st->running) {
            addLine(rows, L("\xC2\xBFTe han invitado? Acepta la invitaci\xC3\xB3n de Steam (con Kenshi abierto; vale el men\xC3\xBA).",
                            "Invited? Accept the Steam invite (with Kenshi open; the main menu is fine)."), COL_GREY);
        } else {
            addButton(rows, st->refuseFinal ? L("Entendido", "OK")
                            : (st->peerPresent || st->waitNote) ? L("Desconectar", "Disconnect")
                                                                : L("Cancelar", "Cancel"),
                      ACT_DISCONNECT);
        }
        addSpace(rows);
        addButton(rows, L("Opciones avanzadas", "Advanced options"), ACT_ADVANCED);
    }
    // One row per wrapped line: the rows have a fixed height, so a line the
    // EditBox wrapped by itself drew over the row below it (944x700 window,
    // 2026-09-25). The width comes from a built row, so the first build of a
    // panel is unwrapped for one tick.
    if (g_panel.open && g_panel.linePx > 0) {
        MyGUI::IFont* font = panelTextFont();
        if (font) {
            // EditBox text padding. Measured at the font's own size even when the
            // rows draw it smaller (accentPanelRows): Kenshi sizes each row from
            // that full-size layout, so a line that only fits when shrunk got a
            // two-row slot with a blank gap under it.
            const float budget = (float)g_panel.linePx - 12.0f;
            std::vector<Row> wrapped;
            std::vector<std::string> parts;
            for (size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].kind != ROW_LINE) { wrapped.push_back(rows[i]); continue; }
                coop::wrapTextPx(uiText(rows[i].text), budget, &panelGlyphAdvance, font, parts);
                for (size_t k = 0; k < parts.size(); ++k)
                    wrapped.push_back(Row(ROW_LINE, parts[k], ACT_NONE, rows[i].col));
            }
            rows.swap(wrapped);
        }
    }
    if ((int)rows.size() > MAX_ROWS) rows.resize(MAX_ROWS);

    std::string sig;
    for (size_t i = 0; i < rows.size(); ++i) {
        char h[16];
        _snprintf(h, sizeof(h) - 1, "%d/%d/%d|", rows[i].kind, rows[i].act, rows[i].col);
        h[sizeof(h) - 1] = '\0';
        sig += h; sig += rows[i].text; sig += '\n';
    }
    if (sig != g_panel.lastSig) g_panel.needsRebuild = true;

    // Create the window once (outside SEH - see the header note on C2712).
    // Layer MUST be "Info": spike 48 proved createFloatingLabel renders non-null
    // there. "Windows" is not a visible MyGUI layer here - the panel is minted
    // and armed but attaches to nothing, so F2 logs open/close yet nothing draws.
    if (!g_panel.panel) {
        std::string layer = "Info";
        g_panel.panel = g->createDatapanel(0.20f, 0.30f, 0.34f, 0.50f, false, layer, true);
        g_panel.built = false;
        if (!g_panel.panel) {
            coop::logErrLine("[coop-ui] createDatapanel FAILED");
        } else if (!uiPanelArmSeh(g, g_panel.panel)) {
            coop::logErrLine("[coop-ui] panel arm (update-list/show) FAILED");
        }
    }

    // (Re)populate the rows when anything visible changed.
    if (g_panel.panel && (g_panel.needsRebuild || !g_panel.built)) {
        std::string title = uiText(L("Co-op    -    F2 para cerrar", "Co-op    -    F2 to close"));
        std::string empty;
        std::vector<std::string> keys(rows.size());
        RowPod pods[MAX_ROWS];
        for (size_t i = 0; i < rows.size(); ++i) rows[i].text = uiText(rows[i].text);
        for (size_t i = 0; i < rows.size(); ++i) {
            char k[16];
            _snprintf(k, sizeof(k) - 1, "kc_row%u", (unsigned)i);
            k[sizeof(k) - 1] = '\0';
            keys[i] = k;
            pods[i].kind = rows[i].kind;
            pods[i].key  = &keys[i];
            pods[i].text = &rows[i].text;
        }
        for (int i = 0; i < MAX_PICK; ++i) g_pickIds[i] = pickIds[i];
        panelBuildSeh(g_panel.panel, &title, pods, (int)rows.size(), &empty);

        // Delegate assignment + colouring live OUTSIDE the SEH frame (pointer
        // targets are valid post-build; assignment can't fault) so no delegate
        // temporary lands in it.
        for (size_t i = 0; i < rows.size(); ++i) {
            if (g_rowBtn[i]) bindButton(g_rowBtn[i], rows[i].act);
            if (g_rowLine[i]) colourRow(g_rowLine[i], rows[i].col);
        }
        accentPanelRows();

        g_panel.built = true;
        g_panel.needsRebuild = false;
        g_panel.lastSig = sig;
        refreshLineWidth();
    }
    // Kenshi's font pass (a resize, the title screen, the font-size option)
    // re-applies every widget's own font, which puts the EditBox lines back on
    // the ASCII-only default font. Buttons and the banner report their font,
    // so they keep the copy; only the rows need this.
    {
        static DWORD s_lastAccent = 0;
        const DWORD now = GetTickCount();
        if (g_panel.built && now - s_lastAccent >= 500) {
            s_lastAccent = now;
            accentPanelRows();
            refreshLineWidth(); // a window resize changes it
        }
    }

    // Connect / disconnect on the Online/Offline edge (edge, not level, so a
    // connect that hasn't reported running yet is not re-fired every tick). The
    // pasted friend id (0 if none) is handed to the plugin, which lets a non-zero
    // value override the config steamPeer; UDP ip/port still come from the config.
    if (g_panel.connectedFlag != g_panel.lastChkVal) {
        g_panel.lastChkVal = g_panel.connectedFlag;
        if (g_panel.connectedFlag && !st->running) {
            char b[80];
            _snprintf(b, sizeof(b) - 1, "[coop-ui] CONNECT role=%s transport=%s",
                      g_panel.hostFlag ? "HOST" : "JOIN",
                      g_panel.steamFlag ? "steam" : "udp");
            b[sizeof(b) - 1] = '\0';
            coop::logLine(b);
            if (onConnect) onConnect(g_panel.hostFlag, g_panel.steamFlag, g_pastedPeer);
        } else if (!g_panel.connectedFlag && st->running) {
            coop::logLine("[coop-ui] DISCONNECT requested");
            if (onDisconnect) onDisconnect();
        }
    }
}

// ---- Persistent co-op status overlay ----------------------------------------
// A fixed banner in the top-left corner of the screen showing live session status
// colored by state (0 = offline/red, 1 = waiting/yellow, 2 = connected/green).
// Unlike the character-tracked ScreenLabel this replaces, it needs no player
// character and holds its place while the camera moves - which also makes it
// visible at the title screen, where a join has no leader while it streams the
// host's world. Removed when show=false.
//
// Two widgets, because neither factory alone does the job. Measured 2026-08-04:
// createFloatingLabel hands back a bare MyGUI::Window that IS visible and
// layer-attached at the coords we ask for, but its skin carries no text region at
// all - setCaption is silently dropped (getCaption().size() stays 0) and it has no
// children, so it draws nothing. It is still the only way to get a widget parented
// to a screen layer instead of to another window, so we keep it as an invisible
// container and put Kenshi's own label factory inside it (createLabelAbs ->
// MyGUI::TextBox with a text-bearing skin, the one the datapanel rows use).
// Caption + colour go to the child; the container is only geometry.

namespace {
// Banner box in pixels: 10 px in from the top-left corner. Applied with the
// absolute setCoord instead of createFloatingLabel's normalized coords, since a
// corner inset is a pixel quantity and the reconstructed header's top/left
// argument order is ambiguous (the normalized values are overwritten either way).
const int kOverlayX = 10;
const int kOverlayY = 10;
const int kOverlayW = 520;
const int kOverlayH = 26;

MyGUI::Window*  g_overlayBox   = 0; // container: geometry + layer attachment
MyGUI::TextBox* g_overlay      = 0; // the label that actually draws the text
int             g_overlayState = -1;
std::string     g_overlayText;

int overlayColorId(int state) { return state == 2 ? 0 : (state == 1 ? 2 : 1); }

// Put the freshly-minted container in its pixel box and mint the label inside it.
// createLabelAbs takes its text by const-ref and MyGUI::Align is a trivial int
// wrapper (no destructor), so this whole frame is SEH-safe - the same rule
// lineColourSeh follows for MyGUI::Colour.
MyGUI::TextBox* overlayBuildSeh(MyGUI::Window* box, const std::string* text) {
    __try {
        box->setCoord(kOverlayX, kOverlayY, kOverlayW, kOverlayH);
        box->setVisible(true);
        MyGUI::TextBox* l = ::gui->createLabelAbs(box, 0, 0, kOverlayW, kOverlayH,
                                                  *text, MyGUI::Align::Left);
        if (l) {
            l->setTextAlign(MyGUI::Align::Left);
            l->setVisible(true);
        }
        return l;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// Caption + colour in place. MyGUI::UString owns a buffer (destructor => C2712),
// so the caller builds it outside this frame and passes a pointer. false = the
// widget faulted; the caller then treats the pointer as dead.
bool overlayUpdateSeh(MyGUI::TextBox* l, const MyGUI::UString* text,
                      const MyGUI::Colour* col) {
    __try {
        l->setCaption(*text);
        l->setTextColour(*col);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Destroying the container takes its label child with it (MyGUI owns the subtree).
void overlayDestroySeh(ForgottenGUI* g, MyGUI::Window* box) {
    __try { g->destroyWidget(box); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
} // namespace

void coopOverlayTick(const char* text, int state, bool show) {
    ForgottenGUI* g = ::gui;
    if (!g) return;

    if (!show) {
        if (g_overlayBox) {
            overlayDestroySeh(g, g_overlayBox);
            g_overlayBox = 0; g_overlay = 0;
            g_overlayState = -1; g_overlayText.clear();
        }
        return;
    }

    ensureAccentFonts();
    std::string t = uiText(text ? std::string(text) : std::string());
    if (!g_overlay) {
        // createFloatingLabel takes the layer BY VALUE (an unwindable temporary
        // => C2712), so the container mint stays outside SEH, exactly like
        // createDatapanel above; ::gui was verified non-null. Layer MUST be "Info"
        // for the same reason the panel uses it - nothing draws on "Windows".
        if (g_overlayBox) { overlayDestroySeh(g, g_overlayBox); g_overlayBox = 0; }
        std::string layer = "Info";
        std::string empty;
        g_overlayBox = g->createFloatingLabel(0.01f, 0.01f, 0.30f, 0.03f, empty,
                                              MyGUI::Align::Default, layer);
        if (!g_overlayBox) {
            coop::logErrLine("[coop-ui] banner container FAILED");
            return;
        }
        g_overlay = overlayBuildSeh(g_overlayBox, &t);
        char b[96];
        _snprintf(b, sizeof(b) - 1, "[coop-ui] banner box=%p label=%p",
                  (void*)g_overlayBox, (void*)g_overlay);
        b[sizeof(b) - 1] = '\0';
        coop::logLine(b);
        if (!g_overlay) {
            coop::logErrLine("[coop-ui] banner label FAILED");
            return;
        }
        useAccentFont(g_overlay);
        g_overlayState = -1;   // no caller state is -1: forces the caption pass
        g_overlayText.clear();
    }

    if (t != g_overlayText || state != g_overlayState) {
        MyGUI::Colour col; markerColour(overlayColorId(state), &col);
        MyGUI::UString u(t.c_str());
        if (overlayUpdateSeh(g_overlay, &u, &col)) {
            g_overlayText = t; g_overlayState = state;
        } else {
            // The GUI destroyed the widgets under us - clearGUI() on a world load
            // empties the layer and notifies nobody, so a pointer held across
            // ticks dangles silently (same lesson as the ScreenLabel registry
            // note above). Forget them and re-mint on the next tick.
            g_overlayBox = 0; g_overlay = 0;
            g_overlayState = -1; g_overlayText.clear();
        }
    }
}

void coopUiShutdown() {
    ForgottenGUI* g = ::gui;
    if (!g) return;
    // exit() off the main thread races the render loop; leave the GUI alone.
    if (g_uiThread != 0 && GetCurrentThreadId() != g_uiThread) return;
    if (g_panel.panel) {
        panelDestroySeh(g, g_panel.panel);
        g_panel.panel = 0; g_panel.built = false;
        clearRowPointers();
        coop::logLine("[coop-ui] panel torn down for process exit");
    }
    g_panel.open = false;
    if (g_overlayBox) {
        overlayDestroySeh(g, g_overlayBox);
        g_overlayBox = 0; g_overlay = 0;
        g_overlayState = -1; g_overlayText.clear();
    }
}

} // namespace engine
} // namespace coop
