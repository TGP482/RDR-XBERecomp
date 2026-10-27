// the game window's hooks: settings, raw mouse input, the cursor and the panel's keys
#pragma once
#include <cstdint>

namespace app {

// GameInfo hooks
void InitInput();
void WindowUp(void* hwnd);
bool WindowMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);

}  // namespace app
