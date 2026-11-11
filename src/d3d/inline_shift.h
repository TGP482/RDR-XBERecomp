// Widescreen: the game's 2D, laid out for 4:3, keeps its shape when pushed inline. While a HUD element draws, it
// goes to its side of the screen; a kept draw stays as the game placed it.
#pragma once

namespace xdk {

// clip space x, -1 at the screen's left to 1 at its right, of the vertices as the game placed them
struct ScreenSpan {
    float x0 = 0;
    float x1 = 0;
    bool drawn = false;
};

// how the 2D drawn meanwhile fits the screen
enum class Fit {
    Narrow,  // laid out for 4:3: narrowed to 4:3, against its side
    Whole,   // a 4:3 picture over the whole screen: narrowed as well
    Shift,   // already in shape, laid out from the middle as for 4:3: moved against its side only
};

// anchor: the clip x that keeps its place, -1 the screen's left edge, 0 its middle, 1 its right edge
void BeginInlineSide(float anchor, Fit fit = Fit::Narrow);
ScreenSpan EndInlineSide();

// 2D drawn meanwhile keeps the game's layout
void BeginInlineKeep();
void EndInlineKeep();

// 2D drawn meanwhile narrows as in a level, even in the front end; 3D stays as it is
void BeginInlineLevel();
void EndInlineLevel();

}  // namespace xdk
