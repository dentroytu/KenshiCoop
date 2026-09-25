// UiLang.h - player-facing text language for the F2 panel and banner: Spanish
// when the Windows UI language is Spanish, English otherwise. Header-only.
// Non-ASCII text is written as UTF-8 byte escapes (the VS2010 compiler reads
// BOM-less sources in the system code page).

#ifndef TOKELACOOP_UILANG_H
#define TOKELACOOP_UILANG_H

#include <windows.h>
#include <string>

namespace coop {

inline bool uiSpanish() {
    static int s = -1;
    if (s < 0) s = ((GetUserDefaultUILanguage() & 0x3FF) == 0x0A /* LANG_SPANISH */) ? 1 : 0;
    return s == 1;
}

inline const char* L(const char* es, const char* en) { return uiSpanish() ? es : en; }

// Suffix of the accented copies of Kenshi's fonts (see accentFontXml).
inline const char* accentFontSuffix() { return "_KC"; }

// Kenshi's data/gui/fonts/kenshi_fonts.xml, turned into the definitions of the
// fonts our panel and banner use instead: every <Resource name="X"> becomes
// "X_KC" and every ASCII-only code range "32 126" becomes "32 255", so the same
// TTF at the same size also rasterizes Latin-1 (accented letters, n-tilde,
// inverted marks). A leading UTF-8 BOM is dropped. *renamed / *widened count
// the edits; the caller treats zero as "the file is not what we expect".
inline std::string accentFontXml(const std::string& src, int* renamed, int* widened) {
    std::string in = src;
    if (in.size() >= 3 && (unsigned char)in[0] == 0xEF &&
        (unsigned char)in[1] == 0xBB && (unsigned char)in[2] == 0xBF)
        in.erase(0, 3);
    std::string out;
    out.reserve(in.size() + 256);
    int nr = 0, nw = 0;
    const std::string nameKey = "name=\"";
    size_t pos = 0;
    for (;;) {
        const size_t n = in.find(nameKey, pos);
        const size_t q = (n == std::string::npos) ? n : in.find('"', n + nameKey.size());
        if (q == std::string::npos) { out.append(in, pos, std::string::npos); break; }
        out.append(in, pos, q - pos);
        out += accentFontSuffix();
        ++nr;
        pos = q;
    }
    const std::string ascii = "<Code range=\"32 126\"/>";
    const std::string latin = "<Code range=\"32 255\"/>";
    for (size_t p = out.find(ascii); p != std::string::npos; p = out.find(ascii, p + latin.size())) {
        out.replace(p, ascii.size(), latin);
        ++nw;
    }
    if (renamed) *renamed = nr;
    if (widened) *widened = nw;
    return out;
}

// Plain-ASCII fallback for UTF-8 text drawn with Kenshi's own fonts, which only
// rasterize codes 32-126: without it every accented letter draws as a gap.
// Latin-1 letters lose their accent (n-tilde becomes n), the inverted question
// and exclamation marks are dropped, a no-break space becomes a space, and any
// other non-ASCII character becomes '?'. Used only when the accented copies of
// Kenshi's fonts (EngineUi.cpp) could not be loaded.
inline std::string foldToAscii(const std::string& s) {
    // U+00C0..U+00FF (UTF-8 C3 80..C3 BF) folded to one ASCII letter each.
    static const char kC3[] =
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out += (char)c; continue; }
        const unsigned char n = (i + 1 < s.size()) ? (unsigned char)s[i + 1] : 0;
        if (c == 0xC3 && n >= 0x80 && n <= 0xBF) { out += kC3[n - 0x80]; ++i; continue; }
        if (c == 0xC2 && n >= 0x80 && n <= 0xBF) {
            if (n == 0xA0) out += ' ';                      // no-break space
            else if (n == 0xAB || n == 0xBB) out += '"';    // guillemets
            else if (n != 0xBF && n != 0xA1) out += '?';    // keep only a mark
            ++i;                                            // inverted ? / ! dropped
            continue;
        }
        // Any other sequence: skip its continuation bytes, show one '?'.
        size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
        i += len - 1;
        out += '?';
    }
    return out;
}

} // namespace coop

#endif // TOKELACOOP_UILANG_H
