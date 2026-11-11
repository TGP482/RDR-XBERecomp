// The main menu's Exit (game/exit_menu.cpp).
#pragma once

#include <cstdint>

namespace game {

// the story page after its update: Exit's row, its press and the answer
void UpdateExitButton(uint32_t page);
// a page about to draw: the warning popup asks the quit question while Exit waits on it
void UpdateExitQuestion(uint32_t page);

}  // namespace game
