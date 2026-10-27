// the monitor the game shows on and the pause while the window is in the background
#pragma once
#include <string>
#include <vector>

#include "app/app.h"

namespace app {

// the connected monitors, in their device numbers' order
struct Display {
    std::string device;  // \\.\DISPLAY1
    int number;          // the device's number, as Windows numbers it
    Mode desktop;
    bool primary;
};
std::vector<Display> Displays();
// the window onto the monitor setting's monitor (the primary for empty or one not connected), then the display
// applied there: the SDK applies the mode to the window's monitor
void ShowOnMonitor();
// any thread that swaps, before the present: waits while the window is in the background or minimized, its sound
// stopped
void WaitWhileUnfocused();

}  // namespace app
