// EngineOwnGuard.cpp - "each player controls only their own characters".
//
// Both games hold both players' characters in one player squad (the drive code
// depends on that), so Kenshi let either player select the friend's characters,
// give them orders and open their inventory. An order ran locally and then fought
// the replication (a stuck "move order", twitching, a job pose dropped), and an
// inventory edit on the friend's character duplicated or lost items.
//
// The guard is a selection scrub: every tick, any of the friend's characters the
// local player has selected is unselected again. A character that cannot stay
// selected cannot be ordered, and its inventory cannot be opened from the
// selection. It touches only PlayerInterface's own selection calls - no detour -
// so the replication's own drive of those bodies is unaffected.
//
// The squad screen is the one place a character is handled without selecting it:
// portraits are dragged between squads. Dropping the friend's character into one
// of our squads made us its owner on both games (publishSquadMoves claims every
// roster edge as our player's action), and dropping one of ours into the friend's
// squad left both games controlling it. So the screen's two drag checks are
// detoured: a drag of the friend's portrait never starts, and none of ours can be
// dropped on the friend's squad.

#include "EngineInternal.h"
#include <kenshi/gui/SquadManagementScreen.h>

namespace coop {
namespace engine {

namespace {

// wraps::DDItemInfo (mygui/common/itembox/ItemDropInfo.h). The handlers take it
// by value, and at 32 bytes the x64 convention passes a pointer to the caller's
// copy - so the detours can take that pointer.
struct DragInfo {
    wraps::BaseLayout* sender;
    size_t             senderIndex;
    wraps::BaseLayout* receiver;
    size_t             receiverIndex;
};
typedef void (__fastcall* PortraitDragFn)(SquadManagementScreen* self,
                                          wraps::BaseLayout* sender,
                                          DragInfo* info, bool* result);

PortraitDragFn   g_startDragOrig    = 0;
PortraitDragFn   g_requestDropOrig  = 0;
bool             g_squadGuardOn     = false;
GameWorld*       g_squadGuardGw     = 0;
SquadBodyClassFn g_squadBodyClass   = 0;
SquadTabClassFn  g_squadTabClass    = 0;
unsigned int     g_refusedFriendsChar = 0;
unsigned int     g_refusedFriendsTab  = 0;

// The handlers are protected, and a pointer to a protected member can only be
// formed inside a derived class.
struct SquadScreenAccess : SquadManagementScreen {
    static intptr_t startDragAddr() {
        return KenshiLib::GetRealAddress(&SquadScreenAccess::notifyStartDropPortrait);
    }
    static intptr_t requestDropAddr() {
        return KenshiLib::GetRealAddress(&SquadScreenAccess::notifyRequestDropPortrait);
    }
};

// The hand of the character behind portrait `index` of a squad's portrait box.
// No SEH here: MyGUI's inline accessors may need unwinding (C2712), so the SEH
// shell is the caller. The range is checked first, so nothing throws.
__declspec(noinline) bool portraitHandAt(wraps::BaseLayout* box, size_t index,
                                         unsigned int out[5]) {
    if (!box) return false;
    SquadManagementScreen::PortraitSquadItemBox* pb =
        static_cast<SquadManagementScreen::PortraitSquadItemBox*>(box);
    MyGUI::ItemBox* ib = pb->getItemBox();
    if (!ib || index >= ib->getItemCount()) return false;
    PortraitData** pd = ib->getItemDataAt<PortraitData*>(index, false);
    if (!pd || !*pd) return false;
    const hand& h = (*pd)->characterHandle;
    out[0] = (unsigned int)h.type; out[1] = h.container; out[2] = h.containerSerial;
    out[3] = h.index; out[4] = h.serial;
    return (out[0] | out[1] | out[2] | out[3] | out[4]) != 0;
}

// Owner class of the dragged portrait's character: 0 unknown, 1 ours, 2 friend's.
int draggedClass(const DragInfo& di) {
    if (!g_squadBodyClass) return 0;
    unsigned int h[5];
    bool ok = false;
    __try { ok = portraitHandAt(di.sender, di.senderIndex, h); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    return ok ? g_squadBodyClass(h) : 0;
}

// Every squad (platoon) seen with a member, and its tab's container. A squad
// emptied on this side keeps its owner: the two games do not always hold the
// same members in the same squad, so the friend's squad can be empty here. Kept
// until the world is swapped (the keys are only compared, never dereferenced).
std::map<ActivePlatoon*, std::pair<unsigned int, unsigned int> > g_platoonTab;
unsigned int g_platoonRefresh = 0;

struct PlatoonSeen { ActivePlatoon* p; unsigned int c, cs; };

unsigned int readPlatoonsSeh(GameWorld* gw, PlatoonSeen* out, unsigned int maxOut) {
    unsigned int n = 0;
    __try {
        PlayerInterface* pl = gw->player;
        if (!pl || !g_getPlatoonFn) return 0;
        unsigned int pc = pl->playerCharacters.size();
        for (unsigned int i = 0; i < pc && n < maxOut; ++i) {
            Character* m = pl->playerCharacters[i];
            if (!m) continue;
            ActivePlatoon* p = g_getPlatoonFn(m);
            unsigned int h[5];
            if (!p || !readObjectHand(static_cast<RootObject*>(m), h)) continue;
            out[n].p = p; out[n].c = h[1]; out[n].cs = h[2];
            ++n;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return n;
}

void notePlatoons(GameWorld* gw) {
    static PlatoonSeen seen[160];   // main-thread only
    const unsigned int n = readPlatoonsSeh(gw, seen, 160);
    for (unsigned int i = 0; i < n; ++i)
        g_platoonTab[seen[i].p] = std::make_pair(seen[i].c, seen[i].cs);
}

// Owner class of the squad a portrait box shows, from its tab's container: 0 when
// the box is not a squad (the dismiss area) or the squad was never seen with a
// member (one the player just created).
int boxSquadClass(wraps::BaseLayout* box) {
    if (!box || !g_squadTabClass || !g_squadGuardGw) return 0;
    ActivePlatoon* p = 0;
    __try {
        SquadManagementScreen::PortraitSquadItemBox* pb =
            static_cast<SquadManagementScreen::PortraitSquadItemBox*>(box);
        if (pb->squad) p = pb->squad->platoon;
    } __except (EXCEPTION_EXECUTE_HANDLER) { p = 0; }
    if (!p) return 0;
    notePlatoons(g_squadGuardGw);   // this drop's members, current
    std::map<ActivePlatoon*, std::pair<unsigned int, unsigned int> >::const_iterator it =
        g_platoonTab.find(p);
    if (it == g_platoonTab.end()) return 0;
    return g_squadTabClass(it->second.first, it->second.second);
}

void logRefusal(const char* what) {
    char b[128];
    _snprintf(b, sizeof(b) - 1, "[own] SQUAD-SCREEN refused: %s", what);
    b[sizeof(b) - 1] = '\0';
    coop::logLine(b);
}

// Picking up a portrait: the friend's characters cannot be dragged at all.
void __fastcall startDragPortrait_hook(SquadManagementScreen* self,
                                       wraps::BaseLayout* sender,
                                       DragInfo* info, bool* result) {
    DragInfo di = { 0, 0, 0, 0 };
    if (info) di = *info;
    g_startDragOrig(self, sender, info, result);
    if (!g_squadGuardOn || !result || !*result || !info) return;
    if (draggedClass(di) != 2) return;
    *result = false;
    ++g_refusedFriendsChar;
    logRefusal("the friend's character cannot be moved here");
}

// Hovering a squad with a portrait: ours cannot go into the friend's squad. The
// box it came from always accepts it back.
void __fastcall requestDropPortrait_hook(SquadManagementScreen* self,
                                         wraps::BaseLayout* sender,
                                         DragInfo* info, bool* result) {
    DragInfo di = { 0, 0, 0, 0 };
    if (info) di = *info;
    g_requestDropOrig(self, sender, info, result);
    if (!g_squadGuardOn || !result || !*result || !info) return;
    if (!di.receiver || di.receiver == di.sender) return;
    if (boxSquadClass(di.receiver) != 2) return;
    *result = false;
    ++g_refusedFriendsTab;
    logRefusal("our character cannot join the friend's squad");
}

} // namespace

bool installSquadScreenGuard(SquadBodyClassFn bodyClass, SquadTabClassFn tabClass) {
    g_squadBodyClass = bodyClass;
    g_squadTabClass  = tabClass;
    intptr_t aStart = SquadScreenAccess::startDragAddr();
    intptr_t aReq   = SquadScreenAccess::requestDropAddr();
    if (!aStart || !aReq) return false;
    if (KenshiLib::AddHook((void*)aStart, (void*)&startDragPortrait_hook,
                           (void**)&g_startDragOrig) != KenshiLib::SUCCESS)
        return false;
    return KenshiLib::AddHook((void*)aReq, (void*)&requestDropPortrait_hook,
                              (void**)&g_requestDropOrig) == KenshiLib::SUCCESS;
}

void setSquadScreenGuard(GameWorld* gw, bool on) {
    g_squadGuardGw = gw;
    g_squadGuardOn = on && gw != 0;
    // Remember which tab each squad is while it still has members (about 2 Hz).
    if (g_squadGuardOn && ++g_platoonRefresh >= 30) {
        g_platoonRefresh = 0;
        notePlatoons(gw);
    }
}

void clearSquadScreenPlatoons() {
    g_platoonTab.clear();
    g_platoonRefresh = 0;
}

void drainSquadScreenRefusals(unsigned int* friendsChar, unsigned int* friendsTab) {
    if (friendsChar) *friendsChar = g_refusedFriendsChar;
    if (friendsTab)  *friendsTab  = g_refusedFriendsTab;
    g_refusedFriendsChar = 0;
    g_refusedFriendsTab  = 0;
}

unsigned int unselectBodies(GameWorld* gw, Character* const* bodies, unsigned int n) {
    if (!gw || !bodies || n == 0) return 0;
    unsigned int done = 0;
    __try {
        PlayerInterface* pl = gw->player;
        if (!pl) return 0;
        for (unsigned int i = 0; i < n; ++i) {
            RootObject* obj = static_cast<RootObject*>(bodies[i]);
            if (!obj || !pl->isObjectSelected(obj)) continue;
            pl->unselectPlayerCharacter(obj);
            ++done;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return done;
}

bool anyBodySelected(GameWorld* gw, Character* const* bodies, unsigned int n) {
    if (!gw || !bodies || n == 0) return false;
    __try {
        PlayerInterface* pl = gw->player;
        if (!pl) return false;
        for (unsigned int i = 0; i < n; ++i)
            if (bodies[i] && pl->isObjectSelected(static_cast<RootObject*>(bodies[i])))
                return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

bool selectBody(GameWorld* gw, Character* c) {
    if (!gw || !c) return false;
    __try {
        PlayerInterface* pl = gw->player;
        if (!pl) return false;
        pl->selectObject(static_cast<RootObject*>(c), /*modifier*/false);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

} // namespace engine
} // namespace coop
