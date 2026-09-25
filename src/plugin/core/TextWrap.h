// TextWrap.h - split one UTF-8 line into lines no wider than a pixel budget,
// breaking at spaces. Pure C++03 so prototest can lock it without the game: the
// plugin passes the F2 panel font's glyph advances.
//
// Why: the panel's rows are Kenshi DatapanelGUI lines of a FIXED height. A line
// longer than the panel is word-wrapped by its EditBox into a second visual line
// that overlaps the next row (seen 2026-09-25 in a 944x700 window: hints drawn
// over the buttons below). Giving each wrapped piece its own row keeps every row
// one line tall.

#ifndef TOKELACOOP_TEXTWRAP_H
#define TOKELACOOP_TEXTWRAP_H

#include <string>
#include <vector>

namespace coop {

// Pixel advance of one Unicode code point in the font being measured.
typedef float (*GlyphAdvanceFn)(unsigned int codepoint, void* ctx);

// Decode the code point at s[*i] and move *i past it. Malformed bytes decode
// as U+FFFD, one byte at a time, so the loop always advances.
inline unsigned int utf8Next(const std::string& s, size_t* i) {
    const unsigned char c = (unsigned char)s[*i];
    unsigned int cp;
    int extra;
    if (c < 0x80)                { cp = c;        extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else                         { ++*i; return 0xFFFD; }
    ++*i;
    for (int k = 0; k < extra; ++k) {
        if (*i >= s.size()) return 0xFFFD;
        const unsigned char d = (unsigned char)s[*i];
        if ((d & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (d & 0x3F);
        ++*i;
    }
    return cp;
}

inline float textWidthPx(const std::string& s, GlyphAdvanceFn adv, void* ctx) {
    float w = 0.0f;
    size_t i = 0;
    while (i < s.size()) w += adv(utf8Next(s, &i), ctx);
    return w;
}

// Words are separated by ' '. A word wider than the budget on its own keeps a
// line of its own (never cut mid-word); maxPx <= 0 means no wrapping. Always
// yields at least one line.
inline void wrapTextPx(const std::string& s, float maxPx, GlyphAdvanceFn adv, void* ctx,
                       std::vector<std::string>& out) {
    out.clear();
    if (maxPx <= 0.0f || textWidthPx(s, adv, ctx) <= maxPx) { out.push_back(s); return; }
    std::string line;
    bool started = false;
    size_t pos = 0;
    for (;;) {
        const size_t sp = s.find(' ', pos);
        const std::string word = s.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        if (!started) {
            line = word;
            started = true;
        } else {
            const std::string cand = line + " " + word;
            if (textWidthPx(cand, adv, ctx) > maxPx) { out.push_back(line); line = word; }
            else line = cand;
        }
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }
    out.push_back(line);
}

} // namespace coop

#endif // TOKELACOOP_TEXTWRAP_H
