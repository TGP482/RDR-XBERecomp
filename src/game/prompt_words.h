#pragma once
#include <string>

// The controls the game's text names in words ("Pull the right trigger", "el stick analógico derecho") as their keys,
// "Press (LMB)", in all five languages; false when the text names none.
bool WithControlKeys(std::u16string& text);
