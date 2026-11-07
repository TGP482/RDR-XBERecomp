// The FPS counter, top left in the game's own font, drawn over everything as a presented frame ends. The frame's
// end puts the texture stages and shaders back after it, as the game's next draws expect.
#include <bit>
#include <cstdio>

#include "app/game_settings.h"
#include "d3d/inline_shift.h"
#include "game/font.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace game_font;

namespace {

// alpha blending on, cdecl(on); blend mode and alpha ref, cdecl(mode, ref); the current ones
constexpr hle::Va kSetAlphaBlend = SYM(SetAlphaBlend);
constexpr hle::Va kSetBlendMode = SYM(SetBlendMode);
constexpr hle::Va kAlphaBlend = SYM(AlphaBlend);
constexpr hle::Va kBlendMode = SYM(BlendMode);
constexpr hle::Va kAlphaRef = SYM(AlphaRef);
constexpr uint32_t kSourceAlpha = 0;  // the subtitles' mode and ref
constexpr uint32_t kSubtitleRef = 0x80;
// a stage's texture, cdecl(stage, texture); the offscreen target being drawn, 0 the back buffer
constexpr hle::Va kSetTexture = SYM(SetStageTexture);
constexpr hle::Va kOffscreenTarget = SYM(OffscreenTarget);

// 640x480 pixels
constexpr float kSize = 12;
constexpr float kMargin = 8;
constexpr size_t kMaxChars = 31;

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
    uint32_t font = FindGameFont(ctx);
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
X86_EXTERN_FUNC(__imp__sub_FrameEnd);
X86_FUNC(sub_FrameEnd) {
    if (app::Rdr().fps_counter && !R32(kOffscreenTarget)) {
        uint32_t ecx = ctx.ecx;
        DrawFpsCounter(ctx);
        ctx.ecx = ecx;
    }
    __imp__sub_FrameEnd(ctx, base);
}
