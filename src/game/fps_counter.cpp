// The FPS counter, top left in the game's own font, drawn over everything as a presented frame ends. The frame's
// end puts the texture stages and shaders back after it, as the game's next draws expect.
#include <bit>
#include <cstdio>

#include "app/game_settings.h"
#include "d3d/inline_shift.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// the default font "berryliumx": a packed one by name and tag (FNT0), cdecl; a loaded one, thiscall(list, name)
constexpr uint32_t kFontName = 0x3CFDA0;
constexpr uint32_t kFontTag = 0x30544E46;
constexpr uint32_t kFindPackedFont = 0x84BB0;
constexpr uint32_t kFindLoadedFont = 0x86FD0;
constexpr uint32_t kLoadedFonts = 0x65EA94;
constexpr uint32_t kLineHeight = 0x1E8;  // font: its line's height in pixels
// text layout: its defaults, fastcall(layout); its colour
constexpr uint32_t kInitLayout = 0x565D0;
constexpr uint32_t kLayoutSize = 0x40;
constexpr uint32_t kLayoutColour = 0x38;
// thiscall(font, x, y, text, scale x, scale y, layout, measure only, batch) ret 0x20
constexpr uint32_t kDrawText = 0x54A30;
// alpha blending on, cdecl(on); blend mode and alpha ref, cdecl(mode, ref); the current ones
constexpr uint32_t kSetAlphaBlend = 0x182080;
constexpr uint32_t kSetBlendMode = 0x182160;
constexpr uint32_t kAlphaBlend = 0x6843B9;
constexpr uint32_t kBlendMode = 0x68435C;
constexpr uint32_t kAlphaRef = 0x6843A2;
constexpr uint32_t kSourceAlpha = 0;  // the subtitles' mode and ref
constexpr uint32_t kSubtitleRef = 0x80;
// a stage's texture, cdecl(stage, texture); the offscreen target being drawn, 0 the back buffer
constexpr uint32_t kSetTexture = 0x181B00;
constexpr uint32_t kOffscreenTarget = 0x683580;

// 640x480 pixels
constexpr float kSize = 12;
constexpr float kMargin = 8;
constexpr size_t kMaxChars = 31;

uint32_t FindFont(X86Context& ctx) {
    uint32_t font = CallGuest(ctx, kFindPackedFont, {kFontName, kFontTag});
    if (font) return font;
    ctx.ecx = kLoadedFonts;
    return CallGuest(ctx, kFindLoadedFont, {kFontName});
}

void DrawText(X86Context& ctx, uint32_t font, uint32_t block, float at, float scale, uint32_t colour) {
    uint32_t layout = block;
    uint32_t text = block + kLayoutSize;
    ctx.ecx = layout;
    CallGuest(ctx, kInitLayout, {});
    W32(layout + kLayoutColour, colour);
    ctx.ecx = font;
    uint32_t xy = std::bit_cast<uint32_t>(at);
    uint32_t s = std::bit_cast<uint32_t>(scale);
    CallGuest(ctx, kDrawText, {xy, xy, text, s, s, layout, 0, 0});
}

void DrawFpsCounter(X86Context& ctx) {
    uint32_t font = FindFont(ctx);
    uint32_t line = font ? R32(font + kLineHeight) : 0;
    static const uint32_t block = mem::AllocSystem(kLayoutSize + (kMaxChars + 1) * 2);
    if (!line || !block) return;
    char text[kMaxChars + 1];
    std::snprintf(text, sizeof(text), "FPS: %.1f", double(app::FrameRate().load()));
    for (size_t i = 0; i < sizeof(text); i++) {
        W16(block + kLayoutSize + uint32_t(i) * 2, uint8_t(text[i]));
        if (!text[i]) break;
    }
    for (uint32_t stage = 1; stage < 4; stage++) CallGuest(ctx, kSetTexture, {stage, 0});
    uint32_t blend = G<uint8_t>(kAlphaBlend);
    uint32_t mode = R32(kBlendMode);
    uint32_t ref = G<uint8_t>(kAlphaRef);
    CallGuest(ctx, kSetAlphaBlend, {1});
    CallGuest(ctx, kSetBlendMode, {kSourceAlpha, kSubtitleRef});
    float scale = kSize / float(line);
    xdk::BeginInlineSide(-1);
    DrawText(ctx, font, block, kMargin + 1, scale, 0xFF000000u);
    DrawText(ctx, font, block, kMargin, scale, 0xFFFFFFFFu);
    xdk::EndInlineSide();
    CallGuest(ctx, kSetBlendMode, {mode, ref});
    CallGuest(ctx, kSetAlphaBlend, {blend});
}

}  // namespace

// the frame's end: resets the stages, streams and shaders, then swaps unless an offscreen target is drawn
X86_EXTERN_FUNC(__imp__sub_0017D3A0);
X86_FUNC(sub_0017D3A0) {
    if (app::Rdr().fps_counter && !R32(kOffscreenTarget)) {
        uint32_t ecx = ctx.ecx;
        DrawFpsCounter(ctx);
        ctx.ecx = ecx;
    }
    __imp__sub_0017D3A0(ctx, base);
}
