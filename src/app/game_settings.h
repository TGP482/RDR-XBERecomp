// settings the SDK's settings file does not know, kept in the same file
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

#include "core/gyro.h"

namespace app {

constexpr uint32_t kGameFov = 63;  // the game camera's own, horizontal degrees on a 4:3 frame

struct RdrSettings {
    std::string monitor;  // its device name, \\.\DISPLAY2; empty the primary
    bool fps_counter = false;
    uint32_t fmv_scale = 0;  // full screen movies on a wider screen: 0 fill it, 1 in a 4:3 frame
    uint32_t fov = kGameFov;  // in play
    uint32_t colour_blind = 0;  // 0 off, 1 protanopia, 2 deuteranopia, 3 tritanopia
    uint32_t contrast = 50;     // 0 to 100, 50 leaves the picture alone
    bool hdr_output = true;  // HDR10 on a display with Windows HDR on
    uint32_t ambient_occlusion = 0;  // 0 off, 1 SSAO, 2 GTAO
    uint32_t msaa = 0;  // samples a pixel of the scene, 0 off
    bool smaa = false;
    bool screen_blur = true;  // the game's screen blur in play
    bool pause_on_focus_loss = true;  // the game and its sound stop while the window is in the background
    bool skip_intros = false;         // the Rockstar logo and titles videos at boot
    uint32_t language = 0;  // 0 automatic (the Windows display language), 1 English, 2 French, 3 German, 4 Italian, 5 Spanish
    uint32_t audio_output = 0;        // audio::kOutputAuto, kOutputStereo, kOutputSurround or kOutputMono
    // the mouse's look, the aim ones times the others while aiming
    float mouse_sensitivity = 1.0f;  // horizontal
    float mouse_vertical_sensitivity = 1.0f;
    float aim_horizontal_sensitivity = 1.0f;
    float aim_vertical_sensitivity = 1.0f;
    bool invert_mouse = false;  // the Y axis
    bool invert_mouse_x = false;
    bool aim_toggle = false;  // a tap of the draw key draws until the next tap
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
// a language setting with automatic resolved; BootLanguage: the setting as this run started, which the game read
uint32_t ResolveLanguage(uint32_t setting);
uint32_t BootLanguage();
// the toggle HUD key's state, not saved
std::atomic<bool>& HudHidden();
// the stick dead zone the game loaded from its pad file, set by the game side
std::atomic<float>& GameDeadZone();
// the frame rate the renderer last measured, for the FPS counter
std::atomic<float>& FrameRate();
// the window's display has Windows HDR on, as the renderer last saw
std::atomic<bool>& HdrDisplay();
// GameInfo hooks: the settings file's toml::table
void LoadRdr(const void* table);
void SaveRdr(void* table);

}  // namespace app
