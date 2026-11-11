// The widescreen HUD (game/hud_widescreen.cpp).
#pragma once

#include <cstdint>

namespace game {

// the view about to draw, thiscall(viewport) of sub_ViewportProjection: its HUD keeps inside its split screen frame
void LatchHudView(uint32_t viewport);

}  // namespace game
