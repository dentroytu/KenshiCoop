// EngineUi.h - narrow PUBLIC engine surface: the in-game co-op session panel +
// status overlay. Carved out of Engine.h (Phase 5a domain split, 2026-07-19) so
// the UI root (Plugin.cpp) includes only what it needs and the sync/replication
// consumers stop transitively seeing the panel API.
//
// Like Engine.h this is a PUBLIC header: it declares only the SEH-guarded engine
// facade and must NEVER pull in a <kenshi/...> internal header - those live in
// the adapter (EngineInternal.h). Forward declarations only.

#ifndef KENSHICOOP_ENGINE_UI_H
#define KENSHICOOP_ENGINE_UI_H

namespace coop {
namespace engine {

// ---- In-game co-op session panel ---------------------------------------------
// A native DatapanelGUI opened with F2 that lets the player pick role + transport
// (buttons/checkboxes - the only reliably interactive DatapanelGUI controls;
// MyGUI comboboxes/editboxes have no usable RVAs and don't receive keyboard focus
// during gameplay) and Connect/Disconnect. The friend's Steam ID is entered by
// clipboard: "Copy my Steam ID" puts the player's own id on the clipboard to
// share, and "Paste friend's Steam ID" reads the friend's id back in (per-session,
// never written to disk). The UDP endpoint (ip/port) still comes from
// coop_config.json. The GUI layer stays session-agnostic: live status is passed IN
// via *st and the user's actions are handed BACK through the callbacks (the plugin
// root owns the session/config wiring). Main-thread only; SEH-guarded.
//
// Steam invite: when the invite layer is up and Steam is the armed transport,
// an "Invite a Steam friend" button opens an in-panel friend list (one button
// per online friend). Picking one sends a Steam lobby invite; the friend accepts
// from their Steam notification and the plugin connects both sides on its own,
// so nobody copies an ID. The list and status come IN via *st; the clicks go
// back OUT through the invite callbacks.
struct CoopFriendRow {
    unsigned long long id;
    const char*        name;  // valid for the duration of the coopPanelTick call
    int                state; // 0 offline, 1 online, 2 playing Kenshi
};
struct CoopPanelState {
    unsigned long long selfSteamId; // steamp2p::selfId (0 = Steam not up)
    unsigned long long peerSteamId; // config steamPeer fallback (0 = unset; pasted id wins)
    bool               running;     // net thread up
    bool               peerPresent; // peer connected
    bool               isHost;      // current armed role (seeds the Host toggle)
    int                transportSel;// current armed transport (0 steam, 1 udp)
    const char*        detail;      // one-line status string for the panel/overlay
    // Join-side save-transfer status (null when not streaming): byte-level
    // progress the one-line detail above has no room for, shown on the F2 panel
    // while a join receives the host's world (e.g. "Streaming host world... 42%
    // (3.1/7.4 MB)"). Set by coopPanelDrive, rendered in dbgVal.
    const char*        transferDetail;
    // Active-mod list check (protocol 56): the "Mods" row text (null until the
    // peer's list arrives), whether it is a mismatch (amber), and the friend's
    // list as mods.cfg text for the "Copy friend's mod list" button (null = none).
    const char*        modsLine;
    bool               modsWarn;
    const char*        peerModsCfg;
    bool               inviteReady;  // Steam invite layer is up
    const char*        inviteStatus; // steaminvite::status() ("" when idle)
    int                inviteCode;   // steaminvite::statusCode() (ST_*), worded by the panel
    const char*        inviteArg;    // steaminvite::statusArg() (invited friend's name)
    int                friendN;      // rows in friends (sorted in-Kenshi > online > offline)
    const CoopFriendRow* friends;
    // Why the connection is not up (core/Refusal.h), worded by the plugin root:
    // a notice (refuseLevel 1 = amber, still trying; 2 = red), a grey hint, and
    // refuseFinal when the JOIN gave up (status "could not connect", button
    // "OK" instead of "Cancel"). Null notice = nothing to explain.
    const char*        refuseNotice;
    const char*        refuseHint;
    int                refuseLevel;
    bool               refuseFinal;
    // At the main menu, connected but not playing yet: 1 = HOST with a friend
    // already in (it must load a game), 2 = JOIN waiting for the host's world.
    int                waitNote;
};
// The panel's role/transport selections at the moment Connect is hit. peerId is the
// Steam ID pasted in-panel this session (0 if none), and overrides the config
// steamPeer in coopUiConnect; the UDP endpoint is re-read from the config there.
typedef void (*CoopConnectFn)(bool isHost, bool useSteam, unsigned long long peerId);
typedef void (*CoopDisconnectFn)();
typedef void (*CoopInviteBeginFn)();                    // open picker + create lobby
typedef void (*CoopInviteFriendFn)(unsigned long long); // send a lobby invite
void coopPanelTick(const CoopPanelState* st, CoopConnectFn onConnect,
                   CoopDisconnectFn onDisconnect, CoopInviteBeginFn onInviteBegin,
                   CoopInviteFriendFn onInviteFriend);

// Persistent co-op connection-status banner: a single screen-space label fixed 10
// px in from the top-left corner (a createFloatingLabel MyGUI::Window on the
// spike-48 screenshot-proven "Info" layer) whose caption shows the live session
// status, colored by state (0 = offline/red, 1 = waiting/yellow, 2 =
// connected/green). Needs no player character, so it also shows at the title
// screen; updated in place when the text/state changes and re-minted if the GUI
// destroyed the widget (world load). Pass show=false to remove it. Main-thread
// only; SEH-guarded.
void coopOverlayTick(const char* text, int state, bool show);

// Process exit: destroy the F2 panel and the banner while the engine's GUI is
// still intact. A panel left on ForgottenGUI's datapanel update list faults the
// engine's own GUI teardown inside exit() (AV at kenshi_x64.exe+0x6ea9ab;
// reproduced 2026-09-24: panel open at close -> crash, closed first -> clean).
// Called from the MSVCR100!exit hook (Plugin.cpp). Main thread only; idempotent.
void coopUiShutdown();

} // namespace engine
} // namespace coop

#endif // KENSHICOOP_ENGINE_UI_H
