#include "game/font.h"

using namespace kernel;

namespace {

// a packed font by name and tag (FNT0), cdecl; a loaded one, thiscall(list, name)
constexpr uint32_t kFontName = 0x3CFDA0;
constexpr uint32_t kFontTag = 0x30544E46;
constexpr uint32_t kFindPackedFont = 0x84BB0;
constexpr uint32_t kFindLoadedFont = 0x86FD0;
constexpr uint32_t kLoadedFonts = 0x65EA94;

}  // namespace

uint32_t FindGameFont(X86Context& ctx) {
    uint32_t font = CallGuest(ctx, kFindPackedFont, {kFontName, kFontTag});
    if (font) return font;
    ctx.ecx = kLoadedFonts;
    return CallGuest(ctx, kFindLoadedFont, {kFontName});
}
