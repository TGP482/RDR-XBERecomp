// settings the SDK's settings file does not know, kept in the same file
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

#include "core/gyro.h"

namespace app {

struct RdrSettings {
    std::string monitor;  // its device name, \\.\DISPLAY2; empty the primary
    bool fps_counter = false;
    uint32_t colour_blind = 0;  // 0 off, 1 protanopia, 2 deuteranopia, 3 tritanopia
    uint32_t contrast = 50;     // 0 to 100, 50 leaves the picture alone
    bool hdr_output = true;  // HDR10 on a display with Windows HDR on
    bool smaa = false;
    bool screen_blur = true;  // the game's screen blur in play
    bool pause_on_focus_loss = true;  // the game and its sound stop while the window is in the background
    bool skip_intros = false;         // the Rockstar logo video at boot
    uint32_t audio_output = 0;        // audio::kOutputAuto, kOutputStereo, kOutputSurround or kOutputMono
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
    float pad_dead_zone = 0.0f;  // both sticks, 0.1 to 1; 0 keeps the game's own
    uint32_t vibration_strength = 100;  // percent of the game's rumble, 10 to 200
    gyro::Settings gyro;
    std::map<std::string, std::string> keys;  // action id to key names, absent is the default
};

RdrSettings& Rdr();
// the toggle HUD key's state, not saved
std::atomic<bool>& HudHidden();
// the stick dead zone the game loaded from its pad file, set by the game side
std::atomic<float>& GameDeadZone();
// the window's display has Windows HDR on, as the renderer last saw
std::atomic<bool>& HdrDisplay();
// GameInfo hooks: the settings file's toml::table
void LoadRdr(const void* table);
void SaveRdr(void* table);

}  // namespace app
