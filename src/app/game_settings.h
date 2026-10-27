// settings the SDK's settings file does not know, kept in the same file
#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "core/gyro.h"

namespace app {

struct RdrSettings {
    std::string monitor;  // its device name, \\.\DISPLAY2; empty the primary
    bool fps_counter = false;
    uint32_t colour_blind = 0;  // 0 off, 1 protanopia, 2 deuteranopia, 3 tritanopia
    bool pause_on_focus_loss = true;  // the game and its sound stop while the window is in the background
    // the mouse's look, the aim ones times the others while aiming
    float mouse_sensitivity = 1.0f;  // horizontal
    float mouse_vertical_sensitivity = 1.0f;
    float aim_horizontal_sensitivity = 1.0f;
    float aim_vertical_sensitivity = 1.0f;
    bool invert_mouse = false;  // the Y axis
    bool invert_mouse_x = false;
    uint32_t walk_speed = 50;  // percent of a full stick while the walk key is held
    // the right stick; the game has its own sensitivity and Y axis invert
    float pad_aim_horizontal_sensitivity = 1.0f;
    float pad_aim_vertical_sensitivity = 1.0f;
    bool invert_pad_x = false;
    gyro::Settings gyro;
    std::map<std::string, std::string> keys;  // action id to key names, absent is the default
};

RdrSettings& Rdr();
// GameInfo hooks: the settings file's toml::table
void LoadRdr(const void* table);
void SaveRdr(void* table);

}  // namespace app
