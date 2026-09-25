// Refusal.h - how a client reads the end of a connection attempt, and what
// each side tells the player about it. Pure and header-only (prototest covers
// it). The codes themselves live in netproto/Wire.h (refuseEncode).
//
// Player text is UTF-8 with byte escapes (see UiLang.h). Keep a hex escape from
// running into a following hex digit: "\xC3\xAD" "a", not "\xC3\xADa".

#ifndef KENSHICOOP_REFUSAL_H
#define KENSHICOOP_REFUSAL_H

#include "../../netproto/Wire.h"

#include <cstdio>
#include <string>

namespace coop {

// How one connection attempt ended, as the client sees it.
enum DropKind {
    DROP_NORMAL    = 0, // a session that was up ended (host left, timeout): reconnect
    DROP_NO_ANSWER = 1, // the host never answered at the ENet level: keep trying
    DROP_SOFT      = 2, // refused with RETRY (session full...): keep trying
    DROP_FINAL     = 3  // refused for good (other version...): stop
};

// A host that predates the refusal codes (v0.53 and older) refuses a different
// version with data 0, right after our HELLO. An ENet timeout cannot come that
// soon: ENet times a peer out no earlier than ENET_PEER_TIMEOUT_MINIMUM (5000 ms)
// after the attempt started, which is why the window is measured from the
// attempt's start and not from the CONNECT event.
const unsigned long LEGACY_WINDOW_MS = 4000;

// Classify the DISCONNECT that ended an attempt. gotConnect: ENet connected on
// this attempt. welcomed: our HELLO was answered with a matching WELCOME.
// *latch receives the code to remember (0 = nothing to show): the host's code,
// or for an older host a VERSION code with an unknown (0) host version.
inline DropKind classifyDrop(u32 data, bool gotConnect, bool welcomed,
                             unsigned long msSinceAttemptStart, u32* latch) {
    *latch = 0;
    if (!gotConnect) return DROP_NO_ANSWER;
    if (refuseReason(data) != 0) {
        *latch = data;
        return refuseRetry(data) ? DROP_SOFT : DROP_FINAL;
    }
    if (!welcomed && data == 0 && msSinceAttemptStart < LEGACY_WINDOW_MS) {
        *latch = refuseEncode(REFUSE_VERSION, false, 0);
        return DROP_FINAL;
    }
    return DROP_NORMAL;
}

// What the panel and banner say. level: 0 = nothing, 1 = amber (still
// trying), 2 = red (stopped). final: the join gave up (status "could not
// connect", button "OK" instead of "Cancel").
struct RefusalText {
    std::string notice;  // one sentence: what happened
    std::string hint;    // what to do next
    std::string banner;  // top-left banner ("" = keep the normal one)
    int         level;
    bool        final;
    RefusalText() : level(0), final(false) {}
};

inline std::string protoNumbers(bool es, u16 ours, u16 theirs) {
    char b[96];
    if (es) _snprintf(b, sizeof(b) - 1, "(C\xC3\xB3" "digo de protocolo: t\xC3\xBA %u, tu amigo %u)",
                      (unsigned)ours, (unsigned)theirs);
    else    _snprintf(b, sizeof(b) - 1, "(Protocol code: you %u, your friend %u)",
                      (unsigned)ours, (unsigned)theirs);
    b[sizeof(b) - 1] = '\0';
    return b;
}

// JOIN: the code the host refused us with (as latched by classifyDrop).
inline RefusalText joinRefusalText(u32 code, u16 ours, bool es) {
    RefusalText t;
    const u8  r  = refuseReason(code);
    const u16 hv = refuseVersion(code);
    if (r == 0) return t;
    if (r == REFUSE_VERSION) {
        t.level = 2; t.final = true;
        t.banner = es ? "Co-op: tu amigo tiene otra versi\xC3\xB3n (pulsa F2)"
                      : "Co-op: your friend has another version (press F2)";
        if (hv == 0) {
            t.notice = es ? "Tu amigo tiene otra versi\xC3\xB3n de KenshiCoop (seguramente m\xC3\xA1s antigua)."
                          : "Your friend has another KenshiCoop version (probably older).";
            t.hint = es ? "P\xC3\xAD" "dele que instale la \xC3\xBAltima y vuelva a abrir Kenshi. "
                          "Luego pulsa Entendido y vuelve a conectar."
                        : "Ask them to install the latest one and restart Kenshi. "
                          "Then press OK and connect again.";
        } else if (hv > ours) {
            t.notice = es ? "Tu amigo tiene una versi\xC3\xB3n m\xC3\xA1s nueva de KenshiCoop."
                          : "Your friend has a newer KenshiCoop version.";
            t.hint = (es ? std::string("Instala la \xC3\xBAltima versi\xC3\xB3n y vuelve a abrir Kenshi. ")
                         : std::string("Install the latest version and restart Kenshi. ")) +
                     protoNumbers(es, ours, hv);
        } else {
            t.notice = es ? "Tu amigo tiene una versi\xC3\xB3n m\xC3\xA1s antigua de KenshiCoop."
                          : "Your friend has an older KenshiCoop version.";
            t.hint = (es ? std::string("P\xC3\xAD" "dele que instale la \xC3\xBAltima y vuelva a abrir Kenshi; "
                                       "luego pulsa Entendido y vuelve a conectar. ")
                         : std::string("Ask them to install the latest one and restart Kenshi; "
                                       "then press OK and connect again. ")) +
                     protoNumbers(es, ours, hv);
        }
        return t;
    }
    if (r == REFUSE_FULL) {
        t.level = 1;
        t.notice = es ? "Tu amigo todav\xC3\xAD" "a tiene abierta tu conexi\xC3\xB3n anterior."
                      : "Your friend's game still holds your previous connection.";
        t.hint = es ? "Espera unos segundos: se reconecta solo. (Si no eres t\xC3\xBA, "
                      "su partida ya tiene 2 jugadores.)"
                    : "Wait a few seconds: it reconnects on its own. (If it is not you, "
                      "their game already has 2 players.)";
        t.banner = es ? "Co-op: esperando a que tu amigo libere la plaza..."
                      : "Co-op: waiting for your friend's game to free the slot...";
        return t;
    }
    if (r == REFUSE_HOST_CLOSED && refuseRetry(code)) {
        t.level = 1;
        t.notice = es ? "Tu amigo ha cerrado o reiniciado la partida."
                      : "Your friend closed or restarted the game.";
        t.hint = es ? "Se reconecta solo cuando vuelva." : "It reconnects on its own when they are back.";
        t.banner = es ? "Co-op: esperando a que tu amigo vuelva..." : "Co-op: waiting for your friend to come back...";
        return t;
    }
    // A reason this build does not know (a newer host): the RETRY bit decides.
    char b[128];
    if (refuseRetry(code)) {
        t.level = 1;
        t.notice = es ? "La partida de tu amigo no puede aceptarte ahora mismo."
                      : "Your friend's game cannot take you right now.";
        t.hint = es ? "Se sigue intentando..." : "Still trying...";
        t.banner = es ? "Co-op: reintentando..." : "Co-op: retrying...";
        return t;
    }
    _snprintf(b, sizeof(b) - 1, es ? "La partida de tu amigo ha rechazado la conexi\xC3\xB3n (motivo %u)."
                                   : "Your friend's game refused the connection (reason %u).", (unsigned)r);
    b[sizeof(b) - 1] = '\0';
    t.level = 2; t.final = true;
    t.notice = b;
    t.hint = es ? "Instalad los dos la \xC3\xBAltima versi\xC3\xB3n de KenshiCoop."
                : "Both of you: install the latest KenshiCoop.";
    t.banner = es ? "Co-op: no se pudo conectar (pulsa F2)" : "Co-op: could not connect (press F2)";
    return t;
}

// JOIN: nobody has answered for a while (no ENet connection at all).
inline RefusalText joinNoAnswerText(bool es, bool steam) {
    RefusalText t;
    t.level = 1;
    t.notice = es ? "Tu amigo todav\xC3\xAD" "a no responde. Seguimos intent\xC3\xA1ndolo..."
                  : "Your friend is not answering yet. Still trying...";
    if (steam)
        t.hint = es ? "Comprueba que tu amigo tiene Kenshi abierto y est\xC3\xA1 conectado como anfitri\xC3\xB3n (F2). "
                      "Si os conect\xC3\xA1is por Steam ID, tiene que haber pegado el tuyo."
                    : "Check that your friend has Kenshi open and is online as host (F2). "
                      "If you connect by Steam ID, they must have pasted yours.";
    else
        t.hint = es ? "Comprueba que tu amigo tiene Kenshi abierto y est\xC3\xA1 conectado como anfitri\xC3\xB3n (F2), "
                      "y la IP y el puerto de coop_config.json."
                    : "Check that your friend has Kenshi open and is online as host (F2), "
                      "and the IP and port in coop_config.json.";
    return t;
}

// HOST: someone tried to join with another protocol version (friendVersion).
inline RefusalText hostRefusedText(u16 friendVersion, u16 ours, bool es) {
    RefusalText t;
    if (friendVersion == 0 || friendVersion == ours) return t;
    t.level = 2;
    if (friendVersion < ours) {
        t.notice = es ? "Tu amigo intent\xC3\xB3 entrar con una versi\xC3\xB3n m\xC3\xA1s antigua de KenshiCoop."
                      : "Your friend tried to join with an older KenshiCoop version.";
        t.hint = (es ? std::string("P\xC3\xAD" "dele que instale la \xC3\xBAltima y vuelva a abrir Kenshi. ")
                     : std::string("Ask them to install the latest one and restart Kenshi. ")) +
                 protoNumbers(es, ours, friendVersion);
    } else {
        t.notice = es ? "Tu amigo intent\xC3\xB3 entrar con una versi\xC3\xB3n m\xC3\xA1s nueva de KenshiCoop."
                      : "Your friend tried to join with a newer KenshiCoop version.";
        t.hint = (es ? std::string("Instala la \xC3\xBAltima versi\xC3\xB3n y vuelve a abrir Kenshi. ")
                     : std::string("Install the latest version and restart Kenshi. ")) +
                 protoNumbers(es, ours, friendVersion);
    }
    t.banner = es ? "Co-op: esperando - tu amigo tiene otra versi\xC3\xB3n (F2)"
                  : "Co-op: waiting - your friend has another version (F2)";
    return t;
}

} // namespace coop

#endif // KENSHICOOP_REFUSAL_H
