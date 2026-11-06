// The cursor on a texture the game draws onto 3D geometry (the front end's book): the draws reading it also write
// where they read it, and the texel under the cursor is read back a few frames later.
#pragma once
#include <cstdint>

namespace xdk {

// texture: the D3D texture's guest header, 0 for none; x, y: the cursor over the window, 0..1
void SetPick(uint32_t texture, float x, float y);
// the texture's coordinates under the cursor, 0..1; false off it
bool Picked(float& u, float& v);

}  // namespace xdk
