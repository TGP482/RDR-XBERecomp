#include "app/window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "app/cursor.h"
#include "app/display.h"
#include "app/menu.h"
#include "core/input.h"

namespace app {

void InitInput() { input::LoadBindings(); }

void WindowUp(void* hwnd) {
    input::StartMouse(hwnd);
    ShowOnMonitor();
}

bool WindowMessage(void*, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    switch (msg) {
        case WM_CHAR:
            input::OnChar(uint32_t(wp), (lp & (1 << 30)) != 0);  // bit 30: a held key's repeat
            return false;
        case WM_INPUT:
            input::OnRawInput(reinterpret_cast<void*>(lp));
            return false;  // the default handling frees the input's buffer
        case WM_SETCURSOR:
            // the settings panel's cursor is ImGui's
            if (LOWORD(lp) != HTCLIENT || MenuOpen()) return false;
            SetCursor(input::CursorShown() ? static_cast<HCURSOR>(MenuCursor()) : nullptr);
            *result = TRUE;
            return true;
        default:
            if (!MenuMessage(msg, wp, lp)) return false;
            *result = 0;
            return true;
    }
}

}  // namespace app
