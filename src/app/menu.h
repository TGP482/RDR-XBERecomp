// the settings panel on F10 and the FPS counter
#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace app {

extern bool g_fps_counter;  // saved as fps_counter

bool MenuOpen();
void ToggleMenu();

// GameInfo hooks
void DrawUi(double fps);
bool MenuMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void MenuPad(int port, input::Pad& pad);

}  // namespace app
