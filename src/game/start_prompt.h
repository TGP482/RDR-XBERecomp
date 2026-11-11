// Enter on the showdown pages' "press start" (game/start_prompt.cpp).
#pragma once

#include <cstdint>

namespace game {

// an active page before its input: its "press start" text showing makes Enter press Start
void UpdateStartPrompt(uint32_t page);

}  // namespace game
