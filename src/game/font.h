#pragma once
#include "xdk/hle.h"

namespace game_font {

constexpr uint32_t kLineHeight = 0x1E8;  // font: its line's height in pixels
// text layout: its defaults, fastcall(layout); its alignment bits, the right edge right aligned text ends on
// (relative to x) and its colour
constexpr hle::Va kInitLayout = SYM(InitLayout);
constexpr uint32_t kLayoutSize = 0x40;
constexpr uint32_t kLayoutAlign = 0x0;
constexpr uint32_t kLayoutRight = 0x18;
constexpr uint32_t kLayoutColour = 0x38;
constexpr uint32_t kAlignRight = 4;
// thiscall(font, x, y, text, scale x, scale y, layout, measure only, batch) ret 0x20, 640x480 pixels
constexpr hle::Va kDrawText = SYM(FontDraw);
// thiscall(font, width out, height out, text, scale x, scale y, layout) ret 0x18
constexpr hle::Va kMeasureText = SYM(MeasureText);

}  // namespace game_font

// the default font "berryliumx", 0 while it is not loaded
uint32_t FindGameFont(X86Context& ctx);
