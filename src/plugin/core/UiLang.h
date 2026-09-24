// UiLang.h - player-facing text language for the F2 panel and banner: Spanish
// when the Windows UI language is Spanish, English otherwise. Header-only.
// Non-ASCII text is written as UTF-8 byte escapes (the VS2010 compiler reads
// BOM-less sources in the system code page).

#ifndef KENSHICOOP_UILANG_H
#define KENSHICOOP_UILANG_H

#include <windows.h>

namespace coop {

inline bool uiSpanish() {
    static int s = -1;
    if (s < 0) s = ((GetUserDefaultUILanguage() & 0x3FF) == 0x0A /* LANG_SPANISH */) ? 1 : 0;
    return s == 1;
}

inline const char* L(const char* es, const char* en) { return uiSpanish() ? es : en; }

} // namespace coop

#endif // KENSHICOOP_UILANG_H
