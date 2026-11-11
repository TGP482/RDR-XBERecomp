// LETTERBOX off: split screen views draw over their frames' borders too (game/fov.cpp), their pictures kept in place.
#pragma once

#include <cstdint>

namespace xdk {

// a viewport in the screen's pixels
struct ViewRect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    bool operator==(const ViewRect&) const = default;
};

// the game's viewport view draws over open instead, its picture where view puts it; open the same as view: as it is
void OpenView(const ViewRect& view, const ViewRect& open);

}  // namespace xdk
