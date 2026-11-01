// Ambient occlusion goes to the renderer as a Clear whose flag is no D3DCLEAR bit, its rects holding the parameters,
// so the render thread keeps it in order with the draws.
#pragma once
#include <cstdint>

namespace xdk {

constexpr uint32_t kClearOcclusion = 0x10000;
// p: 1 / the projection's x and y scale, its x and y centre, then view z = p[4] / (ndc depth - p[5]), then the fog
// already drawn: amount p[8] * saturate(p[6] * view z + p[7]), then p[9]: 1 for GTAO, 0 for SSAO; three rects' worth
constexpr uint32_t kOcclusionParams = 12;

// over the scene drawn so far, from the projection and fog the game set; gtao: GTAO, else SSAO
void Occlusion(bool gtao);

}  // namespace xdk
