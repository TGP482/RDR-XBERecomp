#include "app/game_settings.h"

#include <algorithm>
#include <iterator>

#include <toml++/toml.hpp>

#include "audio/audio.h"

namespace app {

namespace {

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

std::atomic<bool>& HudHidden() {
    static std::atomic<bool> hidden{false};
    return hidden;
}

void LoadRdr(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    RdrSettings& s = Rdr();
    s.monitor = t["monitor"].value_or(s.monitor);
    s.fps_counter = t["fps_counter"].value_or(s.fps_counter);
    s.colour_blind = std::min(t["colour_blind"].value_or(s.colour_blind), 3u);
    s.contrast = std::min(t["contrast"].value_or(s.contrast), 100u);
    s.hdr_output = t["hdr_output"].value_or(s.hdr_output);
    s.smaa = t["smaa"].value_or(s.smaa);
    s.pause_on_focus_loss = t["pause_on_focus_loss"].value_or(s.pause_on_focus_loss);
    s.skip_intros = t["skip_intros"].value_or(s.skip_intros);
    s.audio_output = std::min(t["audio_output"].value_or(s.audio_output), audio::kOutputMono);
    audio::SetOutputMode(s.audio_output);
    s.mouse_sensitivity = ReadFloat(t, "mouse_sensitivity", s.mouse_sensitivity);
    s.mouse_vertical_sensitivity = ReadFloat(t, "mouse_vertical_sensitivity", s.mouse_vertical_sensitivity);
    s.aim_horizontal_sensitivity = ReadFloat(t, "aim_horizontal_sensitivity", s.aim_horizontal_sensitivity);
    s.aim_vertical_sensitivity = ReadFloat(t, "aim_vertical_sensitivity", s.aim_vertical_sensitivity);
    s.invert_mouse = t["invert_mouse"].value_or(s.invert_mouse);
    s.invert_mouse_x = t["invert_mouse_x"].value_or(s.invert_mouse_x);
    s.walk_speed = std::clamp(t["walk_speed"].value_or(s.walk_speed), 10u, 90u);
    s.pad_aim_horizontal_sensitivity =
        ReadFloat(t, "pad_aim_horizontal_sensitivity", s.pad_aim_horizontal_sensitivity);
    s.pad_aim_vertical_sensitivity = ReadFloat(t, "pad_aim_vertical_sensitivity", s.pad_aim_vertical_sensitivity);
    s.invert_pad_x = t["invert_pad_x"].value_or(s.invert_pad_x);
    float zone = ReadFloat(t, "pad_dead_zone", s.pad_dead_zone);
    s.pad_dead_zone = zone > 0.0f ? std::clamp(zone, 0.1f, 1.0f) : 0.0f;
    s.vibration_strength = std::clamp(t["vibration_strength"].value_or(s.vibration_strength), 10u, 200u);
    LoadGyro(t, s.gyro);
    if (const toml::table* keys = t["keys"].as_table()) {
        for (auto&& [id, value] : *keys) {
            if (auto v = value.value<std::string>()) s.keys[std::string(id.str())] = *v;
        }
    }
}

void SaveRdr(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    const RdrSettings& s = Rdr();
    t.insert("monitor", s.monitor);
    t.insert("fps_counter", s.fps_counter);
    t.insert("colour_blind", int64_t(s.colour_blind));
    t.insert("contrast", int64_t(s.contrast));
    t.insert("hdr_output", s.hdr_output);
    t.insert("smaa", s.smaa);
    t.insert("pause_on_focus_loss", s.pause_on_focus_loss);
    t.insert("skip_intros", s.skip_intros);
    t.insert("audio_output", int64_t(s.audio_output));
    t.insert("mouse_sensitivity", double(s.mouse_sensitivity));
    t.insert("mouse_vertical_sensitivity", double(s.mouse_vertical_sensitivity));
    t.insert("aim_horizontal_sensitivity", double(s.aim_horizontal_sensitivity));
    t.insert("aim_vertical_sensitivity", double(s.aim_vertical_sensitivity));
    t.insert("invert_mouse", s.invert_mouse);
    t.insert("invert_mouse_x", s.invert_mouse_x);
    t.insert("walk_speed", int64_t(s.walk_speed));
    t.insert("pad_aim_horizontal_sensitivity", double(s.pad_aim_horizontal_sensitivity));
    t.insert("pad_aim_vertical_sensitivity", double(s.pad_aim_vertical_sensitivity));
    t.insert("invert_pad_x", s.invert_pad_x);
    t.insert("pad_dead_zone", double(s.pad_dead_zone));
    t.insert("vibration_strength", int64_t(s.vibration_strength));
    SaveGyro(t, s.gyro);
    toml::table keys;
    for (const auto& [id, value] : s.keys) keys.insert(id, value);
    t.insert("keys", keys);
}

}  // namespace app
