// The prompt bar's draw (swuiPageFrontendButtons): its rows of several keys lay out as one, so a key wider than its
// icon never runs into the words before it.
#pragma once

#include <cstdint>

namespace game {

void BeginPromptBar(uint32_t page);
void EndPromptBar();

}  // namespace game
