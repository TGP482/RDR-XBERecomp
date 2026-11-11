// The showdown character and level pages start the match on Start alone while their "press start" text shows
// (sub_0002E650, sub_0003F5E0), A picking a character there. Enter, which is A in menus, presses Start there instead.
#include <cstring>

#include "core/input.h"
#include "game/start_prompt.h"
#include "game/swui.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace swui;

void game::UpdateStartPrompt(uint32_t page) {
    for (uint32_t i = 0; i < R32(page + kPageWidgetCount); i++) {
        uint32_t widget = R32(page + kPageWidgets + i * 4);
        if (!widget || !G<uint8_t>(widget + kWidgetShown)) continue;
        const char* name = static_cast<const char*>(HostPtr(widget + kWidgetName));
        if (_stricmp(name, "press start") == 0) input::KeepStartPrompt();
    }
}
