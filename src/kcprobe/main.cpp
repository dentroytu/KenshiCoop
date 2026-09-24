// kcprobe - a fake KenshiCoop peer for testing the handshake on one PC, with
// no second Kenshi and no second Steam account. Test-only; never shipped.
//
//   kcprobe client [options]   connect to a running KenshiCoop host (UDP),
//                              send HELLO with any protocol version, and report
//                              exactly how the host answered.
//
// Client options:
//   --ip A          host address                      (default 127.0.0.1)
//   --port N        host port                         (default 27800)
//   --version N     protocol version to claim in HELLO (default: this build's)
//   --hold MS       after WELCOME, stay connected MS ms (default 0)
//   --exit goodbye|crash
//                   goodbye = disconnect properly (default);
//                   crash   = vanish without a disconnect, like a killed game
//   --junk          before HELLO, send a non-HELLO packet. It is harmless either
//                   way, so the result line cannot show the host's receive
//                   gate: check the host log for "dropped packet type 4 from a
//                   peer not admitted yet".
//   --retry MS      keep retrying every 2 s for up to MS ms until WELCOMEd
//
// One result line per attempt, then the exit code of the last attempt:
//   0 WELCOME, 2 refused VERSION, 3 refused FULL, 4 other refusal,
//   5 disconnected without a reason code, 6 no answer, 1 usage/setup error.
//
// VC10 / C++03, built like tunneltest (scripts\build_kcprobe.cmd).

#define _CRT_SECURE_NO_WARNINGS 1

#include <enet/enet.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../netproto/Wire.h"

using namespace coop;

namespace {

enum Outcome { OUT_WELCOME = 0, OUT_REF_VERSION = 2, OUT_REF_FULL = 3,
               OUT_REF_OTHER = 4, OUT_DROP = 5, OUT_NO_ANSWER = 6 };

struct Opts {
    std::string ip;
    int         port;
    u16         version;
    DWORD       holdMs;
    bool        goodbye;
    bool        junk;
    DWORD       retryMs;
    Opts() : ip("127.0.0.1"), port(27800), version(PROTOCOL_VERSION), holdMs(0),
             goodbye(true), junk(false), retryMs(0) {}
};

const enet_uint8 CH_RELIABLE   = 0;
const enet_uint8 CH_UNRELIABLE = 1;
const int        CH_COUNT      = 3;

DWORD g_start = 0;
DWORD elapsed() { return GetTickCount() - g_start; }

void describe(u32 data, char* out, size_t n) {
    const u8 r = refuseReason(data);
    if (r == 0) { _snprintf(out, n - 1, "data=0x%08X (no reason code)", (unsigned)data); }
    else {
        const char* name = r == REFUSE_VERSION ? "VERSION" : r == REFUSE_FULL ? "FULL" :
                           r == REFUSE_MODS ? "MODS" : r == REFUSE_HOST_CLOSED ? "HOST_CLOSED" : "?";
        _snprintf(out, n - 1, "data=0x%08X reason=%u(%s) retry=%d hostver=%u",
                  (unsigned)data, (unsigned)r, name, refuseRetry(data) ? 1 : 0,
                  (unsigned)refuseVersion(data));
    }
    out[n - 1] = '\0';
}

// Drain events without acting on them (host broadcasts while we hold).
void drain(ENetHost* h, DWORD ms, bool* lost, u32* lostData) {
    const DWORD t0 = GetTickCount();
    ENetEvent ev;
    while ((GetTickCount() - t0) < ms) {
        const int r = enet_host_service(h, &ev, 20);
        if (r <= 0) continue;
        if (ev.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(ev.packet);
        else if (ev.type == ENET_EVENT_TYPE_DISCONNECT) { *lost = true; *lostData = ev.data; return; }
    }
}

// One connect attempt. Returns an Outcome and prints one result line.
int attempt(const Opts& o, int n) {
    ENetHost* h = enet_host_create(0, 1, CH_COUNT, 0, 0);
    if (!h) { std::printf("[%6lu ms] attempt %d: client create failed\n", elapsed(), n); return 1; }
    ENetAddress addr;
    enet_address_set_host(&addr, o.ip.c_str());
    addr.port = (enet_uint16)o.port;
    ENetPeer* peer = enet_host_connect(h, &addr, CH_COUNT, 0);
    if (!peer) { enet_host_destroy(h); std::printf("attempt %d: connect failed\n", n); return 1; }

    int result = OUT_NO_ANSWER;
    bool connected = false, done = false;
    char line[160] = "";
    const DWORD t0 = GetTickCount();
    DWORD connectAt = 0;
    ENetEvent ev;
    // ENet's own connect timeout ends an unanswered attempt (>= 5 s); 20 s cap.
    while (!done && (GetTickCount() - t0) < 20000) {
        const int r = enet_host_service(h, &ev, 20);
        if (r <= 0) continue;
        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            connected = true;
            connectAt = GetTickCount();
            if (o.junk) {
                // A non-HELLO packet first: type byte of an entity batch, too
                // short to parse even if it got through.
                const u8 junk = (u8)PKT_ENTITY_BATCH;
                enet_peer_send(peer, CH_UNRELIABLE, enet_packet_create(&junk, 1, 0));
                enet_host_flush(h);
            }
            HelloPacket hp;
            hp.type = (u8)PKT_HELLO; hp.version = o.version; hp.nameLen = 0;
            enet_peer_send(peer, CH_RELIABLE,
                           enet_packet_create(&hp, sizeof(hp), ENET_PACKET_FLAG_RELIABLE));
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            WelcomePacket w;
            if (packetType(ev.packet->data, (unsigned)ev.packet->dataLength) == PKT_WELCOME &&
                readPacket(ev.packet->data, (unsigned)ev.packet->dataLength, &w)) {
                _snprintf(line, sizeof(line) - 1, "WELCOME id=%u hostver=%u after %lu ms",
                          (unsigned)w.playerId, (unsigned)w.version,
                          (unsigned long)(GetTickCount() - connectAt));
                line[sizeof(line) - 1] = '\0';
                result = OUT_WELCOME;
                done = true;
            }
            enet_packet_destroy(ev.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            char d[128];
            describe(ev.data, d, sizeof(d));
            const u8 reason = refuseReason(ev.data);
            if (!connected) { _snprintf(line, sizeof(line) - 1, "NO ANSWER (%s)", d); result = OUT_NO_ANSWER; }
            else {
                _snprintf(line, sizeof(line) - 1, "REFUSED %s after %lu ms", d,
                          (unsigned long)(GetTickCount() - connectAt));
                result = reason == REFUSE_VERSION ? OUT_REF_VERSION :
                         reason == REFUSE_FULL ? OUT_REF_FULL :
                         reason != 0 ? OUT_REF_OTHER : OUT_DROP;
            }
            line[sizeof(line) - 1] = '\0';
            peer = 0;
            done = true;
            break;
        }
        default: break;
        }
    }
    if (!done) _snprintf(line, sizeof(line) - 1, "NO ANSWER (gave up after 20 s)");
    std::printf("[%6lu ms] attempt %d: %s\n", elapsed(), n, line);

    if (result == OUT_WELCOME && peer) {
        bool lost = false; u32 lostData = 0;
        if (o.holdMs) drain(h, o.holdMs, &lost, &lostData);
        if (lost) {
            char d[128]; describe(lostData, d, sizeof(d));
            std::printf("[%6lu ms]   host dropped us while holding: %s\n", elapsed(), d);
        } else if (o.goodbye) {
            enet_peer_disconnect(peer, 0);
            bool acked = false; u32 ignore = 0;
            drain(h, 1000, &acked, &ignore);
            std::printf("[%6lu ms]   left with goodbye (%s)\n", elapsed(),
                        acked ? "acknowledged" : "not acknowledged in 1 s");
        } else {
            std::printf("[%6lu ms]   left WITHOUT goodbye (simulated crash)\n", elapsed());
        }
    }
    enet_host_destroy(h);
    return result;
}

int usage() {
    std::printf("usage: kcprobe client [--ip A] [--port N] [--version N] [--hold MS]\n"
                "                      [--exit goodbye|crash] [--junk] [--retry MS]\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || std::strcmp(argv[1], "client") != 0) return usage();
    Opts o;
    for (int i = 2; i < argc; ++i) {
        const char* a = argv[i];
        const char* v = (i + 1 < argc) ? argv[i + 1] : 0;
        if      (!std::strcmp(a, "--ip") && v)      { o.ip = v; ++i; }
        else if (!std::strcmp(a, "--port") && v)    { o.port = std::atoi(v); ++i; }
        else if (!std::strcmp(a, "--version") && v) { o.version = (u16)std::atoi(v); ++i; }
        else if (!std::strcmp(a, "--hold") && v)    { o.holdMs = (DWORD)std::atol(v); ++i; }
        else if (!std::strcmp(a, "--retry") && v)   { o.retryMs = (DWORD)std::atol(v); ++i; }
        else if (!std::strcmp(a, "--exit") && v)    { o.goodbye = std::strcmp(v, "crash") != 0; ++i; }
        else if (!std::strcmp(a, "--junk"))         { o.junk = true; }
        else return usage();
    }
    if (enet_initialize() != 0) { std::printf("enet_initialize failed\n"); return 1; }
    g_start = GetTickCount();
    std::printf("kcprobe client -> %s:%d claiming protocol v%u (this build: v%u)\n",
                o.ip.c_str(), o.port, (unsigned)o.version, (unsigned)PROTOCOL_VERSION);
    int n = 1;
    int result = attempt(o, n);
    while (o.retryMs && result != OUT_WELCOME && result != OUT_REF_VERSION &&
           result != 1 && elapsed() < o.retryMs) {
        Sleep(2000);
        result = attempt(o, ++n);
    }
    enet_deinitialize();
    return result;
}
