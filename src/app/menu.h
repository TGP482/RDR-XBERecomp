// the settings panel on F10
#pragma once
#include <cstdint>

namespace app {

bool MenuOpen();
void ToggleMenu();
bool Capturing();  // a key bind waits for a key

void DrawUi(double fps);  // GameInfo::ui
// a key message for the panel: its key, Escape while it is open, the texture replacement key; true takes it
bool MenuMessage(uint32_t msg, uintptr_t wp, intptr_t lp);

}  // namespace app
