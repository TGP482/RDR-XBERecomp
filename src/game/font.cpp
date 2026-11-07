#include "game/font.h"

using namespace kernel;

namespace {

// a packed font by name and tag (FNT0), cdecl; a loaded one, thiscall(list, name)
constexpr hle::Va kFontName = SYM(FontName);
constexpr uint32_t kFontTag = 0x30544E46;
constexpr hle::Va kFindPackedFont = SYM(FindPackedFont);
constexpr hle::Va kFindLoadedFont = SYM(FindLoadedFont);
constexpr hle::Va kLoadedFonts = SYM(LoadedFonts);

}  // namespace

uint32_t FindGameFont(X86Context& ctx) {
    uint32_t font = CallGuest(ctx, kFindPackedFont, {kFontName, kFontTag});
    if (font) return font;
    ctx.ecx = kLoadedFonts;
    return CallGuest(ctx, kFindLoadedFont, {kFontName});
}
