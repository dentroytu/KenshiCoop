// TabLedger.h - who owns each squad tab, kept with the save.
//
// A tab's owner is decided once per session (Replicator::decideTabs). The save's
// own tabs used to be seeded by rank alone: rank 0 the host's, rank 1 the join's,
// every further tab the host's. So a squad the join made mid-session went to the
// host at the next reload or reconnect, and the own-characters guard left no way
// to hand it back. The host now writes each tab's owner to a small file in the
// save folder (the save transfer carries it to the join), and seeding reads it
// before falling back to the rank rule (ledgerDecidesSeeding).
//
// Pure, CRT-only: shared by the plugin and prototest.
#ifndef TOKELACOOP_TABLEDGER_H
#define TOKELACOOP_TABLEDGER_H

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace coop {

// Owner roles as written in the file: the host, or the joining player.
enum { TAB_ROLE_HOST = 0, TAB_ROLE_JOIN = 1 };

// A tab is keyed by its container hand (container, containerSerial).
typedef std::map<std::pair<unsigned int, unsigned int>, unsigned char> TabLedger;

// The file inside the save folder.
inline const char* tabLedgerFileName() { return "TokelaCoop_squads.txt"; }

// "# comment\nv1\n<container> <containerSerial> host|join\n..." with CRLF.
inline std::string formatTabLedger(const TabLedger& ledger) {
    std::string s = "# TokelaCoop: who owns each squad tab (written by the host when it saves)\r\n";
    s += "v1\r\n";
    for (TabLedger::const_iterator it = ledger.begin(); it != ledger.end(); ++it) {
        char b[64];
        _snprintf(b, sizeof(b) - 1, "%u %u %s\r\n", it->first.first, it->first.second,
                  it->second == TAB_ROLE_JOIN ? "join" : "host");
        b[sizeof(b) - 1] = '\0';
        s += b;
    }
    return s;
}

// Lenient: comments, blank lines and malformed rows are skipped; a file without
// the "v1" line (another format) yields nothing. Returns the rows read.
inline unsigned int parseTabLedger(const std::string& text, TabLedger* out) {
    if (out) out->clear();
    bool versioned = false;
    unsigned int n = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' '))
            line.erase(line.size() - 1);
        if (line.empty() || line[0] == '#') continue;
        if (line == "v1") { versioned = true; continue; }
        if (!versioned) continue;
        unsigned long c = 0, cs = 0;
        char role[8] = { 0 };
        if (sscanf(line.c_str(), "%lu %lu %7s", &c, &cs, role) != 3) continue;
        const std::string r(role);
        if (r != "host" && r != "join") continue;
        if (out) (*out)[std::make_pair((unsigned int)c, (unsigned int)cs)] =
                     (unsigned char)(r == "join" ? TAB_ROLE_JOIN : TAB_ROLE_HOST);
        ++n;
    }
    return n;
}

// Does the ledger decide this seeding? Only the join's rows count: when one of
// the tabs being seeded is recorded as the join's, those tabs are the join's and
// every other tab the host's. When none is - a save from before the ledger, a
// save where only the host ever had squads, or the friend's squads are all gone -
// the rank rule decides as it always did, so a squad split off for the friend
// (rank 1) still becomes theirs. A host row alone never decides anything: the
// host's own new squads are recorded too, and must not keep the friend from
// getting one.
inline bool ledgerDecidesSeeding(const TabLedger& ledger,
                                 const std::vector<std::pair<unsigned int, unsigned int> >& tabs) {
    for (size_t i = 0; i < tabs.size(); ++i) {
        TabLedger::const_iterator it = ledger.find(tabs[i]);
        if (it != ledger.end() && it->second == TAB_ROLE_JOIN) return true;
    }
    return false;
}

// The seeded owner of 'tab' when ledgerDecidesSeeding is true.
inline bool ledgerTabIsJoins(const TabLedger& ledger, const std::pair<unsigned int, unsigned int>& tab) {
    TabLedger::const_iterator it = ledger.find(tab);
    return it != ledger.end() && it->second == TAB_ROLE_JOIN;
}

} // namespace coop

#endif
