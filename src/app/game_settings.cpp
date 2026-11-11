#include "app/game_settings.h"

#include <algorithm>
#include <iterator>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "app/window.h"
#include "audio/audio.h"
#include "xdk/hle.h"

namespace app {

namespace {

bool g_fps_cap_saved = false;  // the settings file holds MAX FRAME RATE

// the Windows display language when the game has it, else English
uint32_t SystemLanguage() {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        case LANG_FRENCH: return 2;
        case LANG_GERMAN: return 3;
        case LANG_ITALIAN: return 4;
        case LANG_SPANISH: return 5;
        default: return 1;
    }
}

float ReadFloat(const toml::table& t, const char* key, float fallback) {
    return float(t[key].value_or(double(fallback)));
}

void LoadGyro(const toml::table& t, gyro::Settings& g) {
    g.enabled = t["gyro_aiming"].value_or(g.enabled);
    g.sensitivity = ReadFloat(t, "gyro_sensitivity", g.sensitivity);
    g.axis = std::min<uint32_t>(t["gyro_axis"].value_or(g.axis), gyro::kWorld);
    uint32_t last_button = uint32_t(std::size(gyro::kButtonNames) - 1);
    g.button = std::min(t["gyro_button"].value_or(g.button), last_button);
    g.button_mode = std::min<uint32_t>(t["gyro_button_mode"].value_or(g.button_mode), gyro::kToggle);
    g.recenter_button = std::min(t["gyro_recenter_button"].value_or(g.recenter_button), last_button);
}

void SaveGyro(toml::table& t, const gyro::Settings& g) {
    t.insert("gyro_aiming", g.enabled);
    t.insert("gyro_sensitivity", double(g.sensitivity));
    t.insert("gyro_axis", int64_t(g.axis));
    t.insert("gyro_button", int64_t(g.button));
    t.insert("gyro_button_mode", int64_t(g.button_mode));
    t.insert("gyro_recenter_button", int64_t(g.recenter_button));
}

const char* const kPlayerKeys[kPlayers] = {"player1", "player2", "player3", "player4"};

// "keyboard", "controllerN", both joined by '+', or "none"
std::string PortName(const Port& p) {
    std::string name = p.keyboard ? "keyboard" : "";
    if (p.pad >= 0) name += (name.empty() ? "controller" : "+controller") + std::to_string(p.pad + 1);
    return name.empty() ? "none" : name;
}

Port ParsePort(const std::string& name, Port fallback) {
    Port p;
    p.keyboard = name.find("keyboard") != std::string::npos;
    size_t at = name.find("controller");
    constexpr size_t kDigit = 10;  // after "controller"
    if (at != std::string::npos && at + kDigit < name.size()) {
        int n = name[at + kDigit] - '0';
        if (n >= 1 && n <= 4) p.pad = n - 1;
    }
    return p.keyboard || p.pad >= 0 || name == "none" ? p : fallback;
}

// a hand edited file: a device the earlier player has comes off the later one; an empty player 1 swaps with the first
// player who has a device
void LoadPlayers(const toml::table& t, Port (&players)[kPlayers]) {
    for (int i = 0; i < kPlayers; i++) {
        Port p = ParsePort(t[kPlayerKeys[i]].value_or(PortName(players[i])), players[i]);
        for (int j = 0; j < i; j++) {
            if (players[j].keyboard) p.keyboard = false;
            if (p.pad == players[j].pad) p.pad = -1;
        }
        players[i] = p;
    }
    if (players[0].keyboard || players[0].pad >= 0) return;
    for (Port& p : players) {
        if (!p.keyboard && p.pad < 0) continue;
        std::swap(players[0], p);
        return;
    }
}

}  // namespace

RdrSettings& Rdr() {
    static RdrSettings s;
    return s;
}

std::atomic<bool>& HdrDisplay() {
    static std::atomic<bool> on{false};
    return on;
}

std::atomic<float>& GameDeadZone() {
    static std::atomic<float> zone{0.3f};  // the xbe's own until the pad file loads
    return zone;
}

uint32_t ResolveLanguage(uint32_t setting) { return setting ? setting : SystemLanguage(); }

uint32_t BootLanguage() {
    static const uint32_t language = ResolveLanguage(Rdr().language);
    return HAS(LanguageSetting) ? language : 1;  // English, a release without the setting has one language
}

std::atomic<float>& FrameRate() {
    static std::atomic<float> fps{0};
    return fps;
}

std::atomic<bool>& HudHidden() {
    static std::atomic<bool> hidden{false};
    return hidden;
}

void LoadRdr(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    RdrSettings& s = Rdr();
    g_fps_cap_saved = t.contains("fps_cap");
    s.monitor = t["monitor"].value_or(s.monitor);
    s.fps_counter = t["fps_counter"].value_or(s.fps_counter);
    s.fmv_scale = std::min(t["fmv_scale"].value_or(s.fmv_scale), 1u);
    s.letterbox = t["letterbox"].value_or(s.letterbox);
    s.fov = std::clamp(t["fov"].value_or(s.fov), 45u, 140u);
    s.colour_blind = std::min(t["colour_blind"].value_or(s.colour_blind), 3u);
    s.contrast = std::min(t["contrast"].value_or(s.contrast), 100u);
    s.hdr_output = t["hdr_output"].value_or(s.hdr_output);
    s.ambient_occlusion = std::min(t["ambient_occlusion"].value_or(s.ambient_occlusion), 2u);
    s.msaa = std::min(t["msaa"].value_or(s.msaa), 8u);
    s.smaa = t["smaa"].value_or(s.smaa);
    s.screen_blur = t["screen_blur"].value_or(s.screen_blur);
    s.pause_on_focus_loss = t["pause_on_focus_loss"].value_or(s.pause_on_focus_loss);
    s.skip_intros = t["skip_intros"].value_or(s.skip_intros);
    s.language = std::min(t["language"].value_or(s.language), 5u);
    s.audio_output = std::min(t["audio_output"].value_or(s.audio_output), audio::kOutputMono);
    audio::SetOutputMode(s.audio_output);
    s.mouse_sensitivity = ReadFloat(t, "mouse_sensitivity", s.mouse_sensitivity);
    s.mouse_vertical_sensitivity = ReadFloat(t, "mouse_vertical_sensitivity", s.mouse_vertical_sensitivity);
    s.aim_horizontal_sensitivity = ReadFloat(t, "aim_horizontal_sensitivity", s.aim_horizontal_sensitivity);
    s.aim_vertical_sensitivity = ReadFloat(t, "aim_vertical_sensitivity", s.aim_vertical_sensitivity);
    s.invert_mouse = t["invert_mouse"].value_or(s.invert_mouse);
    s.invert_mouse_x = t["invert_mouse_x"].value_or(s.invert_mouse_x);
    s.aim_toggle = t["aim_toggle"].value_or(s.aim_toggle);
    s.pad_aim_horizontal_sensitivity =
        ReadFloat(t, "pad_aim_horizontal_sensitivity", s.pad_aim_horizontal_sensitivity);
    s.pad_aim_vertical_sensitivity = ReadFloat(t, "pad_aim_vertical_sensitivity", s.pad_aim_vertical_sensitivity);
    s.invert_pad_x = t["invert_pad_x"].value_or(s.invert_pad_x);
    s.aim_assist = t["aim_assist"].value_or(s.aim_assist);
    float zone = ReadFloat(t, "pad_dead_zone", s.pad_dead_zone);
    s.pad_dead_zone = zone > 0.0f ? std::clamp(zone, 0.1f, 1.0f) : 0.0f;
    s.vibration_strength = std::clamp(t["vibration_strength"].value_or(s.vibration_strength), 10u, 200u);
    LoadGyro(t, s.gyro);
    if (const toml::table* keys = t["keys"].as_table()) {
        for (auto&& [id, value] : *keys) {
            if (auto v = value.value<std::string>()) s.keys[std::string(id.str())] = *v;
        }
    }
    LoadPlayers(t, s.players);
    if (const toml::table* pads = t["controller"].as_table()) {
        for (auto&& [id, value] : *pads) {
            if (auto v = value.value<std::string>()) s.pad_buttons[std::string(id.str())] = *v;
        }
    }
}

void SaveRdr(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    const RdrSettings& s = Rdr();
    t.insert("monitor", s.monitor);
    t.insert("fps_counter", s.fps_counter);
    t.insert("fov", int64_t(s.fov));
    t.insert("fmv_scale", int64_t(s.fmv_scale));
    t.insert("letterbox", s.letterbox);
    t.insert("colour_blind", int64_t(s.colour_blind));
    t.insert("contrast", int64_t(s.contrast));
    t.insert("hdr_output", s.hdr_output);
    t.insert("ambient_occlusion", int64_t(s.ambient_occlusion));
    t.insert("msaa", int64_t(s.msaa));
    t.insert("smaa", s.smaa);
    t.insert("screen_blur", s.screen_blur);
    t.insert("pause_on_focus_loss", s.pause_on_focus_loss);
    t.insert("skip_intros", s.skip_intros);
    t.insert("language", int64_t(s.language));
    t.insert("audio_output", int64_t(s.audio_output));
    t.insert("mouse_sensitivity", double(s.mouse_sensitivity));
    t.insert("mouse_vertical_sensitivity", double(s.mouse_vertical_sensitivity));
    t.insert("aim_horizontal_sensitivity", double(s.aim_horizontal_sensitivity));
    t.insert("aim_vertical_sensitivity", double(s.aim_vertical_sensitivity));
    t.insert("invert_mouse", s.invert_mouse);
    t.insert("invert_mouse_x", s.invert_mouse_x);
    t.insert("aim_toggle", s.aim_toggle);
    t.insert("pad_aim_horizontal_sensitivity", double(s.pad_aim_horizontal_sensitivity));
    t.insert("pad_aim_vertical_sensitivity", double(s.pad_aim_vertical_sensitivity));
    t.insert("invert_pad_x", s.invert_pad_x);
    t.insert("aim_assist", s.aim_assist);
    t.insert("pad_dead_zone", double(s.pad_dead_zone));
    t.insert("vibration_strength", int64_t(s.vibration_strength));
    SaveGyro(t, s.gyro);
    toml::table keys;
    for (const auto& [id, value] : s.keys) keys.insert(id, value);
    t.insert("keys", keys);
    for (int i = 0; i < kPlayers; i++) t.insert(kPlayerKeys[i], PortName(s.players[i]));
    toml::table pads;
    for (const auto& [id, value] : s.pad_buttons) pads.insert(id, value);
    t.insert("controller", pads);
}

// after the settings load, with or without a file: MAX FRAME RATE starts at the refresh rate
void InitRdr() {
    if (!g_fps_cap_saved) Get().fps_cap = kFpsCapRefresh;
    InitInput();
}

}  // namespace app
