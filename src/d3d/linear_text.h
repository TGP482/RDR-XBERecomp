// Pixel fonts: text the console samples point by point, one texel to a pixel of its 480 lines.
#pragma once

namespace xdk {

// while on, stage 0 samples linearly where it would point sample: above 480 lines a pixel font's texels magnify
void SetLinearText(bool on);

}  // namespace xdk
