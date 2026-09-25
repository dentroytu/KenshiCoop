// ModList.h - active-mod list compare (protocol 56). Pure, header-only C++03 so
// prototest can lock it without the game: the plugin fills a ModListPacket from
// the engine (engine::activeModList), and the receiver parses both texts and
// diffs them here. Line format: "file|version\n" per mod, in load order.

#ifndef TOKELACOOP_MODLIST_H
#define TOKELACOOP_MODLIST_H

#include <string>
#include <vector>
#include <cstdio>

#include "../../netproto/ContentHash.h" // fnv1aInit/fnv1aUpdate

namespace coop {

struct ModEntry {
    std::string file;    // file name only: a listed path's folder is dropped
    std::string version;
    bool        builtIn; // listed with a path: the game's own data (gamedata.base, ...)
    ModEntry() : builtIn(false) {}
};

// Split a ModListPacket text into entries. Tolerates a missing trailing
// newline and a line without '|' (version left empty); skips empty lines.
//
// The engine lists the game's own data files by full path ("C:\...\Kenshi\data\
// gamedata.base") and mods by name. Two installs in different folders are the
// same game, so only the file name is compared - comparing paths reported the
// four base files as "4 missing, 4 extra" for every friend whose Kenshi lives
// elsewhere. Done on the receiving side so it also holds for older senders.
inline void parseModText(const char* text, unsigned int len, std::vector<ModEntry>& out) {
    out.clear();
    std::string line;
    for (unsigned int i = 0; i <= len; ++i) {
        char c = (i < len) ? text[i] : '\n';
        if (c == '\0') c = '\n';
        if (c != '\n') { line += c; continue; }
        if (!line.empty()) {
            ModEntry e;
            std::string::size_type bar = line.rfind('|');
            if (bar == std::string::npos) { e.file = line; }
            else { e.file = line.substr(0, bar); e.version = line.substr(bar + 1); }
            std::string::size_type slash = e.file.find_last_of("\\/");
            if (slash != std::string::npos) { e.file = e.file.substr(slash + 1); e.builtIn = true; }
            out.push_back(e);
        }
        line.clear();
        if (i < len && text[i] == '\0') break;
    }
}

inline unsigned int modTextHash(const char* text, unsigned int len) {
    return fnv1aUpdate(fnv1aInit(), text, len);
}

struct ModDiff {
    std::vector<std::string> missing;     // the peer has it, we don't
    std::vector<std::string> extra;       // we have it, the peer doesn't
    std::vector<std::string> versionDiff; // both have it, versions differ
    int  orderAt;                         // first load-order position (1-based, over the
                                          // mods both have) that differs; 0 = same order
    bool same() const {
        return missing.empty() && extra.empty() && versionDiff.empty() && orderAt == 0;
    }
    ModDiff() : orderAt(0) {}
};

inline int findModFile(const std::vector<ModEntry>& v, const std::string& file) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].file == file) return (int)i;
    return -1;
}

inline ModDiff diffModLists(const std::vector<ModEntry>& mine,
                            const std::vector<ModEntry>& theirs) {
    ModDiff d;
    std::vector<std::string> mineCommon, theirsCommon;
    for (size_t i = 0; i < theirs.size(); ++i) {
        int j = findModFile(mine, theirs[i].file);
        if (j < 0) { d.missing.push_back(theirs[i].file); continue; }
        theirsCommon.push_back(theirs[i].file);
        if (mine[j].version != theirs[i].version)
            d.versionDiff.push_back(theirs[i].file + " (you " + mine[j].version +
                                    ", friend " + theirs[i].version + ")");
    }
    for (size_t i = 0; i < mine.size(); ++i) {
        if (findModFile(theirs, mine[i].file) < 0) d.extra.push_back(mine[i].file);
        else mineCommon.push_back(mine[i].file);
    }
    for (size_t i = 0; i < mineCommon.size() && i < theirsCommon.size(); ++i) {
        if (mineCommon[i] != theirsCommon[i]) { d.orderAt = (int)i + 1; break; }
    }
    return d;
}

// One line for the F2 panel / log, e.g. "2 missing, 1 extra, order differs at #7"
// (Spanish with es=true, for the panel; logs stay English).
inline std::string summarizeModDiff(const ModDiff& d, bool es = false) {
    if (d.same()) return es ? "iguales" : "match";
    std::string s;
    char b[48];
    if (!d.missing.empty()) {
        _snprintf(b, sizeof(b) - 1, es ? "te faltan %u" : "%u missing", (unsigned)d.missing.size());
        b[sizeof(b) - 1] = '\0'; s += b;
    }
    if (!d.extra.empty()) {
        _snprintf(b, sizeof(b) - 1, es ? "%ste sobran %u" : "%s%u extra", s.empty() ? "" : ", ",
                  (unsigned)d.extra.size());
        b[sizeof(b) - 1] = '\0'; s += b;
    }
    if (!d.versionDiff.empty()) {
        _snprintf(b, sizeof(b) - 1, es ? "%s%u con otra versi\xC3\xB3n" : "%s%u other version",
                  s.empty() ? "" : ", ", (unsigned)d.versionDiff.size());
        b[sizeof(b) - 1] = '\0'; s += b;
    }
    if (d.orderAt != 0) {
        _snprintf(b, sizeof(b) - 1, es ? "%sorden distinto desde el #%d" : "%sorder differs at #%d",
                  s.empty() ? "" : ", ", d.orderAt);
        b[sizeof(b) - 1] = '\0'; s += b;
    }
    return s;
}

// The peer's list as mods.cfg content (one file name per line, load order), so
// a player can copy it and match their launcher's mod list. The game's own data
// files are never in mods.cfg, so they are left out.
inline std::string modsCfgText(const std::vector<ModEntry>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i].builtIn) continue;
        s += v[i].file; s += "\r\n";
    }
    return s;
}

} // namespace coop

#endif // TOKELACOOP_MODLIST_H
