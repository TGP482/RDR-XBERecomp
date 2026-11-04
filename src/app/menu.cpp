// the settings panel on F10
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <imgui.h>

#include "app/app.h"
#include "app/display.h"
#include "app/game_settings.h"
#include "app/menu.h"
#include "app/panel_colors.h"
#include "app/translations.h"
#include "audio/audio.h"
#include "core/input.h"

namespace app {

namespace {

std::atomic<bool> g_open{false};

constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"OFF", "2x", "4x", "8x", "16x"};
constexpr uint32_t kMsaaValues[] = {0, 2, 4, 8};
const char* const kMsaaNames[] = {"OFF", "2x", "4x", "8x"};
const char* const kAoNames[] = {"OFF", "SSAO", "GTAO"};
constexpr uint32_t kFpsCaps[] = {30, 60, 75, 90, 120, 144, 165, 240, 360, kFpsCapRefresh, 0};
// in DisplayMode order
const char* const kDisplayModeNames[] = {"FULLSCREEN", "BORDERLESS", "WINDOWED"};
const char* const kDumpFormatNames[] = {"PNG", "DDS"};
const char* const kFmvScaleNames[] = {"FILL", "4:3"};
const char* const kColourBlindNames[] = {"OFF", "PROTANOPIA", "DEUTERANOPIA", "TRITANOPIA"};
constexpr uint32_t kAudioOutputs[] = {audio::kOutputAuto, audio::kOutputMono, audio::kOutputStereo,
                                      audio::kOutputSurround};
// automatic, then the game's languages
const char* const kLanguageNames[] = {"AUTOMATIC", "ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH"};
const char* const kAudioOutputNames[] = {"AUTOMATIC", "MONO", "STEREO", "SURROUND"};

// render resolution in percent of the output
constexpr int kRenderMin = 20;
constexpr int kRenderMax = 200;
constexpr int kRenderStep = 5;

enum class RowId {
    Heading,
    Bind,
    Monitor,
    DisplayMode,
    Resolution,
    RenderResolution,
    Fov,
    Vsync,
    HdrOutput,
    MaxFps,
    FpsCounter,
    FmvScale,
    ColourBlind,
    Contrast,
    Aniso,
    AmbientOcclusion,
    Msaa,
    Smaa,
    ScreenBlur,
    PauseOnFocusLoss,
    Language,
    SkipIntros,
    AudioOutput,
    TextureReplacement,
    TextureDumping,
    DumpFormat,
    MouseHorizontal,
    MouseVertical,
    AimHorizontal,
    AimVertical,
    InvertMouseX,
    InvertMouseY,
    AimToggle,
    PadAimHorizontal,
    PadAimVertical,
    InvertPadX,
    AimAssist,
    PadDeadZone,
    VibrationStrength,
    GyroAiming,
    GyroSensitivity,
    GyroAxis,
    GyroButton,
    GyroButtonMode,
    GyroRecenter,
    GyroCalibrate,
};

struct Row {
    RowId id;
    const char* label;
    size_t action = 0;  // a bind row's input action
};

constexpr Row kSettingsRows[] = {
    {RowId::Heading, "DISPLAY"},
    {RowId::Monitor, "MONITOR"},
    {RowId::DisplayMode, "DISPLAY MODE"},
    {RowId::Resolution, "RESOLUTION"},
    {RowId::RenderResolution, "RENDER RESOLUTION"},
    {RowId::Fov, "FIELD OF VIEW"},
    {RowId::Vsync, "VERTICAL SYNC"},
    {RowId::HdrOutput, "HDR OUTPUT"},
    {RowId::MaxFps, "MAX FRAME RATE"},
    {RowId::FpsCounter, "FPS COUNTER"},
    {RowId::FmvScale, "FMV SCALE"},
    {RowId::ColourBlind, "COLOUR BLIND MODE"},
    {RowId::Contrast, "CONTRAST"},
    {RowId::Heading, "GRAPHICS"},
    {RowId::AmbientOcclusion, "AMBIENT OCCLUSION"},
    {RowId::Msaa, "MSAA"},
    {RowId::Smaa, "SMAA"},
    {RowId::Aniso, "ANISOTROPIC FILTERING"},
    {RowId::ScreenBlur, "SCREEN BLUR"},
    {RowId::Heading, "GENERAL"},
    {RowId::SkipIntros, "SKIP INTROS"},
    {RowId::PauseOnFocusLoss, "PAUSE ON FOCUS LOSS"},
    {RowId::Language, "LANGUAGE"},
    {RowId::AudioOutput, "AUDIO MODE"},
    {RowId::Heading, "TEXTURES"},
    {RowId::TextureReplacement, "TEXTURE REPLACEMENT"},
    {RowId::TextureDumping, "TEXTURE DUMPING"},
    {RowId::DumpFormat, "DUMP FORMAT"},
};

constexpr Row kMouseRows[] = {
    {RowId::Heading, "MOUSE"},
    {RowId::MouseHorizontal, "HORIZONTAL SENSITIVITY"},
    {RowId::MouseVertical, "VERTICAL SENSITIVITY"},
    {RowId::AimHorizontal, "AIM HORIZONTAL SENSITIVITY"},
    {RowId::AimVertical, "AIM VERTICAL SENSITIVITY"},
    {RowId::InvertMouseX, "INVERT X-AXIS"},
    {RowId::InvertMouseY, "INVERT Y-AXIS"},
    {RowId::AimToggle, "AIM TOGGLE"},
};

// the game has its own sensitivity, Y axis invert and vibration in its options
constexpr Row kControllerRows[] = {
    {RowId::Heading, "GENERAL"},
    {RowId::PadAimHorizontal, "AIM HORIZONTAL SENSITIVITY"},
    {RowId::PadAimVertical, "AIM VERTICAL SENSITIVITY"},
    {RowId::InvertPadX, "INVERT X-AXIS"},
    {RowId::AimAssist, "AIM ASSIST"},
    {RowId::PadDeadZone, "DEADZONE"},
    {RowId::VibrationStrength, "VIBRATION STRENGTH"},
    {RowId::Heading, "GYRO"},
    {RowId::GyroAiming, "GYRO AIMING"},
    {RowId::GyroSensitivity, "GYRO SENSITIVITY"},
    {RowId::GyroAxis, "GYRO AXIS"},
    {RowId::GyroButton, "GYRO BUTTON"},
    {RowId::GyroButtonMode, "GYRO BUTTON MODE"},
    {RowId::GyroRecenter, "RECENTER BUTTON"},
    {RowId::GyroCalibrate, "CALIBRATE GYRO"},
};

const char* const kPages[] = {"RECOMP SETTINGS", "MOUSE AND KEYBOARD", "CONTROLLER"};
constexpr int kSettingsPage = 0;
constexpr int kKeyboardPage = 1;
constexpr int kControllerPage = 2;

// the mouse rows, then each section of the key binds under its title
const std::vector<Row>& KeyboardRows() {
    static const std::vector<Row> rows = [] {
        std::vector<Row> out(std::begin(kMouseRows), std::end(kMouseRows));
        size_t count = 0;
        size_t sections = 0;
        const input::Action* actions = input::Actions(&count);
        const input::Section* titles = input::Sections(&sections);
        for (size_t i = 0, s = 0; i < count; i++) {
            if (s < sections && titles[s].first == i) out.push_back({RowId::Heading, titles[s++].title});
            out.push_back({RowId::Bind, actions[i].label, i});
        }
        return out;
    }();
    return rows;
}

std::vector<Row> PageRows(int page) {
    if (page == kKeyboardPage) return KeyboardRows();
    if (page == kControllerPage) return {std::begin(kControllerRows), std::end(kControllerRows)};
    return {std::begin(kSettingsRows), std::end(kSettingsRows)};
}

// in the game's language, as ImGui's UTF-8; caps for the panel's capitals
std::string Ui(std::string_view text, bool caps = true) {
    static std::map<std::string, std::string> cache;
    std::string key = std::string(text) + (caps ? "\1" : "\2");
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    std::string out;
    for (char16_t c : Translate(text)) {
        // latin 1 lower case to upper; 0xF7 is the division sign
        if (caps && ((c >= u'a' && c <= u'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7))) c -= 0x20;
        if (c < 0x80) {
            out += char(c);
        } else if (c < 0x800) {
            out += char(0xC0 | c >> 6);
            out += char(0x80 | (c & 0x3F));
        } else {
            out += char(0xE0 | c >> 12);
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }
    return cache[key] = out;
}

// Values

int Wrap(int index, int step, int count) {
    index = (index + step) % count;
    return index < 0 ? index + count : index;
}

template <size_t N>
int NearestIndex(const uint32_t (&values)[N], uint32_t value) {
    int best = 0;
    uint32_t best_d = UINT32_MAX;
    for (int i = 0; i < int(N); i++) {
        uint32_t d = values[i] > value ? values[i] - value : value - values[i];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

template <size_t N>
void StepTable(const uint32_t (&values)[N], uint32_t& value, int step) {
    value = values[Wrap(NearestIndex(values, value), step, int(N))];
}

// value clamped to lo to hi, stepped by inc and wrapping around
int StepRange(int value, int lo, int hi, int inc, int step) {
    int count = (hi - lo) / inc + 1;
    return lo + inc * Wrap((std::clamp(value, lo, hi) - lo) / inc, step, count);
}

uint32_t StepRenderPercent(uint32_t percent, int step) {
    return uint32_t(StepRange(int(percent), kRenderMin, kRenderMax, kRenderStep, step));
}

// 0.1 to 20.0 in steps of 0.1, shown as five times the stored value so the game's own reads 5.0
void StepSensitivity(float& v, int step) {
    v = float(StepRange(int(std::lround(v * 50.0f)), 1, 200, 1, step)) / 50.0f;
}

// 0.1 to 5.0 in steps of 0.1
void StepGyroSensitivity(float& v, int step) {
    v = float(StepRange(int(std::lround(v * 10.0f)), 1, 50, 1, step)) / 10.0f;
}

template <size_t N>
void StepName(uint32_t& value, const char* const (&)[N], int step) {
    value = uint32_t(Wrap(int(std::min<size_t>(value, N - 1)), step, int(N)));
}

template <size_t N>
const char* NameText(uint32_t value, const char* const (&names)[N]) {
    return names[std::min<size_t>(value, N - 1)];
}

bool SameMode(const Mode& a, const Mode& b) { return a.width == b.width && a.height == b.height; }

// the display rows' settings, which wait for Apply on the panel
struct DisplayChoice {
    std::string monitor;
    DisplayMode mode;
    uint32_t width;
    uint32_t height;
    bool operator==(const DisplayChoice&) const = default;
};

DisplayChoice CurrentDisplay() {
    const Settings& s = Get();
    return {Rdr().monitor, s.display_mode, s.width, s.height};
}

// the monitor's modes, smallest first so Left steps down; the primary's for empty
std::vector<Mode> ReadModes(const std::string& monitor) {
    std::wstring device(monitor.begin(), monitor.end());
    std::vector<Mode> modes;
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(device.empty() ? nullptr : device.c_str(), i, &dm); i++) {
        if (dm.dmBitsPerPel < 32 || dm.dmPelsWidth < 640) continue;
        Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        auto same = [&](const Mode& o) { return SameMode(o, m); };
        if (std::none_of(modes.begin(), modes.end(), same)) modes.push_back(m);
    }
    std::sort(modes.begin(), modes.end(), [](const Mode& a, const Mode& b) {
        return a.width != b.width ? a.width < b.width : a.height < b.height;
    });
    return modes;
}

const std::vector<Mode>& Modes(const std::string& monitor) {
    static std::map<std::string, std::vector<Mode>> cache;
    auto [it, fresh] = cache.try_emplace(monitor);
    if (fresh) it->second = ReadModes(monitor);
    return it->second;
}

// the setting's monitor in the list, the primary's index when it is not connected
int MonitorIndex(const std::vector<Display>& list, const std::string& monitor) {
    int primary = 0;
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].device == monitor) return int(i);
        if (list[i].primary) primary = int(i);
    }
    return primary;
}

Mode MonitorDesktop(const std::string& monitor) {
    std::vector<Display> list = Displays();
    return list.empty() ? Desktop() : list[size_t(MonitorIndex(list, monitor))].desktop;
}

Mode ShownMode(const DisplayChoice& d) {
    return d.width && d.height ? Mode{d.width, d.height} : MonitorDesktop(d.monitor);
}

void StepResolution(DisplayChoice& d, int step) {
    const std::vector<Mode>& modes = Modes(d.monitor);
    if (modes.empty()) return;
    Mode now = ShownMode(d);
    int index = 0;
    for (size_t i = 0; i < modes.size(); i++) {
        if (SameMode(modes[i], now)) index = int(i);
    }
    Mode m = modes[size_t(Wrap(index, step, int(modes.size())))];
    // the desktop's own mode is kept as 0, which follows the desktop
    bool native = SameMode(m, MonitorDesktop(d.monitor));
    d.width = native ? 0 : m.width;
    d.height = native ? 0 : m.height;
}

// another monitor starts on its own desktop mode
void StepMonitor(DisplayChoice& d, int step) {
    std::vector<Display> list = Displays();
    if (list.empty()) return;
    const Display& next = list[size_t(Wrap(MonitorIndex(list, d.monitor), step, int(list.size())))];
    if (next.device == d.monitor) return;
    d.monitor = next.device;
    d.width = 0;
    d.height = 0;
}

void StepDisplay(RowId id, int step, DisplayChoice& d) {
    if (id == RowId::Monitor) StepMonitor(d, step);
    else if (id == RowId::DisplayMode) d.mode = DisplayMode(Wrap(int(d.mode), step, 3));
    else StepResolution(d, step);
}

std::string ModeText(const Mode& m) { return std::to_string(m.width) + "x" + std::to_string(m.height); }

std::string MonitorText(const DisplayChoice& d) {
    std::vector<Display> list = Displays();
    if (list.empty()) return "PRIMARY";
    const Display& m = list[size_t(MonitorIndex(list, d.monitor))];
    return std::to_string(m.number) + ": " + ModeText(m.desktop) + (m.primary ? " (PRIMARY)" : "");
}

std::string DisplayText(RowId id, const DisplayChoice& d) {
    if (id == RowId::Monitor) return MonitorText(d);
    if (id == RowId::DisplayMode) return kDisplayModeNames[std::min(uint32_t(d.mode), 2u)];
    return ModeText(ShownMode(d));
}

// the setting an ON / OFF row flips, nullptr for any other row
bool* ToggleFlag(Settings& s, RowId id) {
    RdrSettings& r = Rdr();
    switch (id) {
        case RowId::Vsync: return &s.vsync;
        case RowId::FpsCounter: return &r.fps_counter;
        case RowId::HdrOutput: return &r.hdr_output;
        case RowId::Smaa: return &r.smaa;
        case RowId::ScreenBlur: return &r.screen_blur;
        case RowId::PauseOnFocusLoss: return &r.pause_on_focus_loss;
        case RowId::SkipIntros: return &r.skip_intros;
        case RowId::TextureReplacement: return &s.custom_textures;
        case RowId::TextureDumping: return &s.dump_textures;
        case RowId::InvertMouseX: return &r.invert_mouse_x;
        case RowId::InvertMouseY: return &r.invert_mouse;
        case RowId::AimToggle: return &r.aim_toggle;
        case RowId::InvertPadX: return &r.invert_pad_x;
        case RowId::AimAssist: return &r.aim_assist;
        case RowId::GyroAiming: return &r.gyro.enabled;
        default: return nullptr;
    }
}

// the setting a sensitivity row steps, nullptr for any other row
float* Sensitivity(RowId id) {
    RdrSettings& r = Rdr();
    switch (id) {
        case RowId::MouseHorizontal: return &r.mouse_sensitivity;
        case RowId::MouseVertical: return &r.mouse_vertical_sensitivity;
        case RowId::AimHorizontal: return &r.aim_horizontal_sensitivity;
        case RowId::AimVertical: return &r.aim_vertical_sensitivity;
        case RowId::PadAimHorizontal: return &r.pad_aim_horizontal_sensitivity;
        case RowId::PadAimVertical: return &r.pad_aim_vertical_sensitivity;
        case RowId::GyroSensitivity: return &r.gyro.sensitivity;
        default: return nullptr;
    }
}

void StepGyro(RowId id, int step) {
    gyro::Settings& g = Rdr().gyro;
    switch (id) {
        case RowId::GyroAxis: StepName(g.axis, gyro::kAxisNames, step); break;
        case RowId::GyroButton: StepName(g.button, gyro::kButtonNames, step); break;
        case RowId::GyroButtonMode: StepName(g.button_mode, gyro::kButtonModeNames, step); break;
        case RowId::GyroRecenter: StepName(g.recenter_button, gyro::kButtonNames, step); break;
        case RowId::GyroCalibrate: gyro::StartCalibration(); break;
        default: break;
    }
}

void Step(RowId id, int step, Settings& s) {
    if (id == RowId::HdrOutput && !HdrDisplay()) return;
    if (bool* flag = ToggleFlag(s, id)) {
        *flag = !*flag;
        return;
    }
    if (float* v = Sensitivity(id)) {
        if (id == RowId::GyroSensitivity) StepGyroSensitivity(*v, step);
        else StepSensitivity(*v, step);
        return;
    }
    switch (id) {
        case RowId::RenderResolution: s.render_percent = StepRenderPercent(s.render_percent, step); break;
        case RowId::MaxFps: StepTable(kFpsCaps, s.fps_cap, step); break;
        case RowId::Aniso: StepTable(kAnisoValues, s.aniso, step); break;
        case RowId::AmbientOcclusion: StepName(Rdr().ambient_occlusion, kAoNames, step); break;
        case RowId::Msaa: StepTable(kMsaaValues, Rdr().msaa, step); break;
        case RowId::DumpFormat: s.texture_dump_format = uint32_t(Wrap(int(s.texture_dump_format), step, 2)); break;
        case RowId::FmvScale: StepName(Rdr().fmv_scale, kFmvScaleNames, step); break;
        case RowId::Language: StepName(Rdr().language, kLanguageNames, step); break;
        case RowId::ColourBlind: StepName(Rdr().colour_blind, kColourBlindNames, step); break;
        case RowId::Contrast: Rdr().contrast = uint32_t(StepRange(int(Rdr().contrast), 0, 100, 1, step)); break;
        case RowId::Fov: Rdr().fov = uint32_t(StepRange(int(Rdr().fov), 45, 140, 1, step)); break;
        case RowId::AudioOutput:
            StepTable(kAudioOutputs, Rdr().audio_output, step);
            audio::SetOutputMode(Rdr().audio_output);
            break;
        // 0.1 to 1 in steps of 0.1, from the game's own while it is kept
        case RowId::PadDeadZone: {
            float& zone = Rdr().pad_dead_zone;
            float shown = zone > 0.0f ? zone : GameDeadZone().load();
            zone = float(StepRange(int(std::lround(shown * 10.0f)), 1, 10, 1, step)) / 10.0f;
            break;
        }
        // 10% to 200% in steps of 10
        case RowId::VibrationStrength: {
            uint32_t& v = Rdr().vibration_strength;
            v = uint32_t(StepRange(int(v), 10, 200, 10, step));
            break;
        }
        default: StepGyro(id, step); break;
    }
}

std::string FpsCapText(uint32_t cap) {
    if (cap == kFpsCapRefresh) return "REFRESH RATE";
    return cap ? std::to_string(cap) : "UNLIMITED";
}

std::string RenderText(const Settings& s) {
    Mode out = ShownMode(CurrentDisplay());
    Mode render{out.width * s.render_percent / 100, out.height * s.render_percent / 100};
    return std::to_string(s.render_percent) + "% (" + ModeText(render) + ")";
}

std::string SensitivityText(float v) {
    char text[16];
    std::snprintf(text, sizeof(text), "%.1f", v);
    return text;
}

std::string GyroText(RowId id) {
    const gyro::Settings& g = Rdr().gyro;
    switch (id) {
        case RowId::GyroAxis: return NameText(g.axis, gyro::kAxisNames);
        case RowId::GyroButton: return NameText(g.button, gyro::kButtonNames);
        case RowId::GyroButtonMode: return NameText(g.button_mode, gyro::kButtonModeNames);
        case RowId::GyroRecenter: return NameText(g.recenter_button, gyro::kButtonNames);
        case RowId::GyroCalibrate: return gyro::Calibrating() ? "KEEP STILL" : "START";
        default: return "";
    }
}

std::string Text(RowId id, Settings& s) {
    if (id == RowId::HdrOutput && !HdrDisplay()) return "NOT SUPPORTED";
    if (bool* flag = ToggleFlag(s, id)) return *flag ? "ON" : "OFF";
    if (float* v = Sensitivity(id)) {
        if (id == RowId::GyroSensitivity) return SensitivityText(*v);
        return SensitivityText(std::clamp(*v * 5.0f, 0.1f, 20.0f));
    }
    switch (id) {
        case RowId::RenderResolution: return RenderText(s);
        case RowId::MaxFps: return FpsCapText(kFpsCaps[NearestIndex(kFpsCaps, s.fps_cap)]);
        case RowId::Aniso: return kAnisoNames[NearestIndex(kAnisoValues, s.aniso)];
        case RowId::AmbientOcclusion: return NameText(Rdr().ambient_occlusion, kAoNames);
        case RowId::Msaa: return kMsaaNames[NearestIndex(kMsaaValues, Rdr().msaa)];
        case RowId::DumpFormat: return kDumpFormatNames[std::min(s.texture_dump_format, 1u)];
        case RowId::FmvScale: return NameText(Rdr().fmv_scale, kFmvScaleNames);
        case RowId::Language: {
            bool restart = ResolveLanguage(Rdr().language) != BootLanguage();
            return std::string(NameText(Rdr().language, kLanguageNames)) + (restart ? " (RESTART)" : "");
        }
        case RowId::ColourBlind: return NameText(Rdr().colour_blind, kColourBlindNames);
        case RowId::Fov: {
            uint32_t v = Rdr().fov;
            return std::to_string(v) + (v == kGameFov ? " (DEFAULT)" : "");
        }
        case RowId::Contrast: {
            uint32_t v = Rdr().contrast;
            return std::to_string(v) + (v == 50 ? " (DEFAULT)" : "");
        }
        case RowId::AudioOutput: return kAudioOutputNames[NearestIndex(kAudioOutputs, Rdr().audio_output)];
        case RowId::PadDeadZone: {
            float zone = Rdr().pad_dead_zone;
            if (zone > 0.0f) return SensitivityText(zone);
            return SensitivityText(GameDeadZone()) + " (DEFAULT)";
        }
        case RowId::VibrationStrength: {
            uint32_t v = Rdr().vibration_strength;
            return std::to_string(v) + (v == 100 ? "% (DEFAULT)" : "%");
        }
        default: return GyroText(id);
    }
}

// Changes

// the monitor, display mode and resolution wait for Apply, as Windows' display settings do; closing drops them
std::atomic<bool> g_display_held{false};
DisplayChoice g_held;

bool DisplayRow(RowId id) { return id == RowId::Monitor || id == RowId::DisplayMode || id == RowId::Resolution; }

void HoldDisplay(RowId id, int step) {
    if (!g_display_held) g_held = CurrentDisplay();
    StepDisplay(id, step, g_held);
    g_display_held = !(g_held == CurrentDisplay());
}

void ApplyHeld() {
    Settings& s = Get();
    Rdr().monitor = g_held.monitor;
    s.display_mode = g_held.mode;
    s.width = g_held.width;
    s.height = g_held.height;
    g_display_held = false;
    ShowOnMonitor();
    Save();
}

void ChangeRow(RowId id, int step) {
    if (DisplayRow(id)) {
        HoldDisplay(id, step);
        return;
    }
    Step(id, step, Get());
    Save();
}

// each page's own rows back to their defaults, the others' kept
void RestoreDefaults(int page) {
    RdrSettings& r = Rdr();
    RdrSettings fresh;
    if (page == kKeyboardPage) {
        r.mouse_sensitivity = fresh.mouse_sensitivity;
        r.mouse_vertical_sensitivity = fresh.mouse_vertical_sensitivity;
        r.aim_horizontal_sensitivity = fresh.aim_horizontal_sensitivity;
        r.aim_vertical_sensitivity = fresh.aim_vertical_sensitivity;
        r.invert_mouse = fresh.invert_mouse;
        r.invert_mouse_x = fresh.invert_mouse_x;
        r.aim_toggle = fresh.aim_toggle;
        input::ResetBindings();
        return;
    }
    if (page == kControllerPage) {
        r.pad_aim_horizontal_sensitivity = fresh.pad_aim_horizontal_sensitivity;
        r.pad_aim_vertical_sensitivity = fresh.pad_aim_vertical_sensitivity;
        r.invert_pad_x = fresh.invert_pad_x;
        r.aim_assist = fresh.aim_assist;
        r.pad_dead_zone = fresh.pad_dead_zone;
        r.vibration_strength = fresh.vibration_strength;
        r.gyro = fresh.gyro;
        Save();
        return;
    }
    std::string join_ip = Get().join_ip;  // no row on the panel
    Get() = Settings{};
    Get().join_ip = join_ip;
    r.monitor = fresh.monitor;
    r.fps_counter = fresh.fps_counter;
    r.fov = fresh.fov;
    r.fmv_scale = fresh.fmv_scale;
    r.colour_blind = fresh.colour_blind;
    r.contrast = fresh.contrast;
    r.hdr_output = fresh.hdr_output;
    r.ambient_occlusion = fresh.ambient_occlusion;
    r.msaa = fresh.msaa;
    r.smaa = fresh.smaa;
    r.screen_blur = fresh.screen_blur;
    r.pause_on_focus_loss = fresh.pause_on_focus_loss;
    r.skip_intros = fresh.skip_intros;
    r.language = fresh.language;
    r.audio_output = fresh.audio_output;
    audio::SetOutputMode(r.audio_output);
    g_display_held = false;
    ShowOnMonitor();
    Save();
}

// shown dimmed and left as it is
bool Greyed(RowId id) {
    if (id == RowId::HdrOutput) return !HdrDisplay();
    return id == RowId::DumpFormat && !Get().dump_textures;
}

// Key binds

// the row being rebound, -1 when idle; its keys must be released before the next press binds
std::atomic<int> g_capture{-1};
int g_capture_slot = 0;  // its box: 0 the key, 1 the alternate
int g_hovered_box = -1;  // the box under the mouse or focused with a key in it, this frame
bool g_capture_armed = false;
int g_capture_done = 0;  // frame of the last bind, the key that bound must not start another
int g_page = kSettingsPage;

std::string KeyText(std::string keys) {
    if (keys.empty()) return "UNBOUND";
    for (char& c : keys) c = char(std::toupper(uint8_t(c)));
    return keys;
}

// the action's key and its alternate as their boxes show them, the alternate blank when there is none
std::vector<std::string> KeyBoxes(size_t action) {
    std::string keys = input::Binding(action);
    size_t comma = keys.find(',');
    std::string second = comma == std::string::npos ? std::string() : keys.substr(comma + 1);
    second = second.substr(0, second.find(','));
    return {KeyText(keys.substr(0, comma)), second.empty() ? second : KeyText(second)};
}

// Escape cancels, any other bindable key or mouse button binds
void PollCapture() {
    if (g_capture < 0) return;
    int vk = input::PressedKey();
    if (!g_capture_armed) {
        g_capture_armed = !vk;
        return;
    }
    if (!vk) return;
    if (vk != VK_ESCAPE) input::Bind(size_t(g_capture.load()), vk, g_capture_slot);
    g_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
    if (!MenuOpen()) input::SetBlocked(false);  // started from the game's options
}

// Drawing

constexpr float kValueEms = 12.0f;  // the value column's width
constexpr float kKeyBoxEms = 10.0f;  // a key box's width, the key and its alternate side by side

// a row's place and size: the label left, the value block (an arrow each side of the value) right
struct RowBox {
    float row_w;
    float row_h;
    float value_w;
    float block_w;
    float row_x;
    float row_y;
};

RowBox MeasureRow() {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box;
    box.row_w = ImGui::GetContentRegionAvail().x;
    box.row_h = ImGui::GetFrameHeight();
    box.value_w = ImGui::GetFontSize() * kValueEms;
    box.block_w = box.row_h * 2 + box.value_w + style.ItemSpacing.x * 2;
    box.row_x = ImGui::GetCursorPosX();
    box.row_y = ImGui::GetCursorPosY();
    return box;
}

void RowLabel(const RowBox& box, const char* label) {
    ImGui::SetCursorPos(ImVec2(box.row_x + ImGui::GetStyle().ItemInnerSpacing.x, box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Ui(label).c_str());
}

// the cursor back to the row's start, then past the whole row
void EndRow(const RowBox& box) {
    ImGui::SetCursorPos(ImVec2(box.row_x, box.row_y));
    ImGui::Dummy(ImVec2(box.row_w, box.row_h));
}

void Section(const char* title) {
    float em = ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(0, em * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(Ui(title).c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, em * 0.2f));
}

// the focused row's arrow keys and dpad, as a step
int FocusedStep() {
    bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);
    bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
    return int(right) - int(left);
}

// the value centred between its arrows; a click on one is a step
int ValueBlock(const RowBox& box, const char* english) {
    std::string shown = Ui(english);
    const char* value = shown.c_str();
    const ImGuiStyle& style = ImGui::GetStyle();
    int step = 0;
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_ButtonRepeat, true);  // held arrows keep stepping
    float block_x = box.row_x + box.row_w - box.block_w;
    ImGui::SetCursorPos(ImVec2(block_x, box.row_y));
    if (ImGui::ArrowButton("##dec", ImGuiDir_Left)) step = -1;
    float value_x = block_x + box.row_h + style.ItemSpacing.x;
    float pad = (box.value_w - ImGui::CalcTextSize(value).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(value_x + std::max(pad, 0.0f), box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value);
    ImGui::SetCursorPos(ImVec2(value_x + box.value_w + style.ItemSpacing.x, box.row_y));
    if (ImGui::ArrowButton("##inc", ImGuiDir_Right)) step = 1;
    ImGui::PopItemFlag();
    return step;
}

// label left, arrows and value right, the row is the only nav stop
int OptionRow(const char* label, const char* value) {
    RowBox box = MeasureRow();
    ImGui::PushID(label);
    int step = 0;
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(box.row_w, box.row_h));
    if (ImGui::IsItemFocused()) {
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                  ImGui::GetColorU32(ImGuiCol_Header));
        step = FocusedStep();
    }
    RowLabel(box, label);
    if (int clicked = ValueBlock(box, value)) step = clicked;
    ImGui::PopID();
    EndRow(box);
    return step;
}

// one key box: a click or Enter starts its capture, a right click or Delete clears it
struct KeyBox {
    bool pressed = false;
    bool cleared = false;
};

KeyBox DrawKeyBox(float x, float y, float w, float h, const std::string& key, bool capturing) {
    KeyBox result;
    ImGui::SetCursorPos(ImVec2(x, y));
    result.pressed = ImGui::InvisibleButton("##key", ImVec2(w, h));
    bool focused = ImGui::IsItemFocused();
    bool delete_key = focused && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
    result.cleared = ImGui::IsItemClicked(ImGuiMouseButton_Right) || delete_key;
    bool lit = capturing || focused || ImGui::IsItemHovered();
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                              ImGui::GetColorU32(lit ? ImGuiCol_Header : ImGuiCol_FrameBg));
    std::string text = Ui(capturing ? "PRESS A KEY" : key);
    float pad = (w - ImGui::CalcTextSize(text.c_str()).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(x + std::max(pad, 0.0f), y));
    ImGui::AlignTextToFramePadding();
    if (capturing) ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(text.c_str());
    if (capturing) ImGui::PopStyleColor();
    return result;
}

// label left, the key and its alternate in a box each right, ending where the other rows' values do
void BindRow(const Row& row) {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box = MeasureRow();
    float value_end = box.row_x + box.row_w - box.block_w + box.row_h + style.ItemSpacing.x + box.value_w;
    float box_w = ImGui::GetFontSize() * kKeyBoxEms;
    std::vector<std::string> keys = KeyBoxes(row.action);
    bool idle = g_capture < 0 && ImGui::GetFrameCount() > g_capture_done + 1;
    ImGui::PushID(row.label);
    RowLabel(box, row.label);
    for (int i = 0; i < 2; i++) {
        ImGui::PushID(i);
        float x = value_end - float(2 - i) * box_w - float(1 - i) * style.ItemSpacing.x;
        bool capturing = g_capture == int(row.action) && g_capture_slot == i;
        KeyBox k = DrawKeyBox(x, box.row_y, box_w, box.row_h, keys[size_t(i)], capturing);
        if ((ImGui::IsItemHovered() || ImGui::IsItemFocused()) && !keys[size_t(i)].empty()) g_hovered_box = i;
        if (k.pressed && idle) {
            g_capture_slot = i;
            g_capture = int(row.action);
            g_capture_armed = false;
        }
        // a Delete that answered a capture is no clear
        if (k.cleared && idle) input::Bind(row.action, 0, i);
        ImGui::PopID();
    }
    ImGui::PopID();
    EndRow(box);
}

void DrawRows(int page) {
    for (const Row& r : PageRows(page)) {
        if (r.id == RowId::Heading) {
            Section(r.label);
            continue;
        }
        if (r.id == RowId::Bind) {
            BindRow(r);
            continue;
        }
        bool held = g_display_held && DisplayRow(r.id);
        std::string value = DisplayRow(r.id) ? DisplayText(r.id, held ? g_held : CurrentDisplay()) : Text(r.id, Get());
        bool greyed = Greyed(r.id);
        ImGui::BeginDisabled(greyed);
        int step = OptionRow(r.label, value.c_str());
        ImGui::EndDisabled();
        if (step && !greyed) ChangeRow(r.id, step);
    }
}

// one tab a page, as wide as they share the line; LB / RB step them as the game's menus do
void DrawTabs() {
    int step = ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) - ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
    // a selection made here shows from the next frame; not under a bind
    bool set = step && g_capture < 0;
    if (set) g_page = Wrap(g_page, step, int(std::size(kPages)));
    ImGui::PushStyleColor(ImGuiCol_Tab, kTab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, kTabHovered);
    ImGui::PushStyleColor(ImGuiCol_TabSelected, kHighlight);
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, kHeading);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.0f);
    if (ImGui::BeginTabBar("##pages")) {
        int count = int(std::size(kPages));
        float gaps = ImGui::GetStyle().ItemInnerSpacing.x * float(count - 1);
        float w = (ImGui::GetContentRegionAvail().x - gaps) / float(count);
        for (int i = 0; i < count; i++) {
            ImGui::SetNextItemWidth(w);
            ImGuiTabItemFlags flags = set && i == g_page ? ImGuiTabItemFlags_SetSelected : 0;
            std::string tab = Ui(kPages[i]) + "###" + kPages[i];
            if (ImGui::BeginTabItem(tab.c_str(), nullptr, flags)) {
                if (!set && i != g_page) {
                    g_page = i;
                    g_capture = -1;
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

const char* Hint() {
    if (g_capture >= 0) return "Press a key or mouse button to bind, Escape cancels";
    if (g_page == kKeyboardPage && g_hovered_box >= 0) return "Right click or Delete clears the key";
    return "";
}

// Back, Defaults and Apply on the right
void DrawButtons() {
    float bw = ImGui::GetFontSize() * 8;
    float pad = ImGui::GetContentRegionAvail().x - bw * 3 - ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(pad, 0.0f));
    if (ImGui::Button((Ui("Back", false) + "###back").c_str(), ImVec2(bw, 0))) ToggleMenu();
    ImGui::SameLine();
    if (ImGui::Button((Ui("Defaults", false) + "###defaults").c_str(), ImVec2(bw, 0))) {
        g_capture = -1;
        RestoreDefaults(g_page);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_display_held);
    if (ImGui::Button((Ui("Apply", false) + "###apply").c_str(), ImVec2(bw, 0))) ApplyHeld();
    ImGui::EndDisabled();
}

void DrawTitle(const char* title) {
    float pad = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(title).x) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(pad, 0.0f));
    ImGui::TextUnformatted(title);
}

void DrawPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 50, em * 32), ImGuiCond_Always);
    for (const auto& [col, color] : kPanelColors) ImGui::PushStyleColor(col, color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.6f, em * 1.2f));
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##settings", nullptr, kFlags)) {
        DrawTitle(Ui("SETTINGS").c_str());
        DrawTabs();
        float footer = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + em;
        ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
        g_hovered_box = -1;
        DrawRows(g_page);
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, kDim);
        ImGui::TextUnformatted(Ui(Hint(), false).c_str());
        ImGui::PopStyleColor();
        DrawButtons();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(int(std::size(kPanelColors)));
}

void ToggleTextureReplacement() {
    Get().custom_textures = !Get().custom_textures;
    Save();
}

}  // namespace

static_assert(std::size(kPages) == kPanelPages);

const char* PanelPageTitle(int page) { return kPages[page]; }

namespace {

bool BindHeading(const std::vector<Row>& rows, size_t i) {
    return rows[i].id == RowId::Heading && i + 1 < rows.size() && rows[i + 1].id == RowId::Bind;
}

// the game's options list the key binds as one KEYBOARD page: the first of their headings starts it, the rest none
const char* SectionTitle(const std::vector<Row>& rows, size_t i) {
    if (rows[i].id != RowId::Heading) return nullptr;
    if (!BindHeading(rows, i)) return rows[i].label;
    for (size_t j = 0; j < i; j++) {
        if (BindHeading(rows, j)) return nullptr;
    }
    return "KEYBOARD";
}

}  // namespace

std::vector<std::string> PanelSections(int page) {
    std::vector<std::string> out;
    std::vector<Row> rows = PageRows(page);
    for (size_t i = 0; i < rows.size(); i++) {
        if (const char* title = SectionTitle(rows, i)) out.push_back(title);
    }
    return out;
}

namespace {

// the rows under the section's heading
std::vector<Row> SectionRows(int page, size_t section) {
    std::vector<Row> out;
    std::vector<Row> rows = PageRows(page);
    size_t heading = 0;
    bool inside = false;
    for (size_t i = 0; i < rows.size(); i++) {
        if (SectionTitle(rows, i)) inside = heading++ == section;
        else if (inside && rows[i].id != RowId::Heading) out.push_back(rows[i]);
    }
    return out;
}

std::string BindText(size_t action) {
    if (g_capture == int(action)) return "PRESS A KEY";
    std::vector<std::string> keys = KeyBoxes(action);
    return keys[1].empty() ? keys[0] : keys[0] + ", " + keys[1];
}

}  // namespace

std::vector<PanelRow> PanelRows(int page, size_t section) {
    std::vector<PanelRow> out;
    for (const Row& r : SectionRows(page, section)) {
        if (r.id == RowId::Bind) out.push_back({r.label, BindText(r.action), true});
        else if (DisplayRow(r.id)) out.push_back({r.label, DisplayText(r.id, CurrentDisplay())});
        else out.push_back({r.label, Text(r.id, Get())});
    }
    return out;
}

// the display rows apply at once, there is no Apply here
void StepPanelRow(int page, size_t section, size_t row, int step) {
    std::vector<Row> rows = SectionRows(page, section);
    if (row >= rows.size() || rows[row].id == RowId::Bind || Greyed(rows[row].id)) return;
    Mode before = ShownMode(CurrentDisplay());
    ChangeRow(rows[row].id, step);
    if (!g_display_held) return;
    // the resolution stops at its ends, Right never goes down
    Mode after = ShownMode(g_held);
    bool up = std::pair(after.width, after.height) > std::pair(before.width, before.height);
    if (rows[row].id == RowId::Resolution && up != (step > 0)) g_display_held = false;
    else ApplyHeld();
}

// the game's input waits with it, as under the panel
void PressPanelRow(int page, size_t section, size_t row, int slot) {
    std::vector<Row> rows = SectionRows(page, section);
    if (row >= rows.size() || rows[row].id != RowId::Bind || g_capture >= 0) return;
    g_capture_slot = slot;
    g_capture_armed = false;
    g_capture = int(rows[row].action);
    input::SetBlocked(true);
}

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

bool Capturing() { return g_capture >= 0; }

void ToggleMenu() {
    bool open = !MenuOpen();
    g_open.store(open, std::memory_order_relaxed);
    g_display_held = false;
    g_capture = -1;
    input::SetBlocked(open);
}

void DrawUi(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    // the cursor is ImGui's only over the panel, the game's own the rest of the time
    if (MenuOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.KeyRepeatRate = 0.1f;  // held arrows step ten times a second, ImGui's twenty is too fast for settings
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    FrameRate() = float(fps);
    PollCapture();
    if (MenuOpen()) DrawPanel();
}

// The settings panel key opens and closes, Escape closes; F10 is a system key, so Windows' menu key handling is kept
// away from it. The texture replacement and HUD keys flip theirs. A key that answers a capture does neither.
bool MenuMessage(uint32_t msg, uintptr_t wp, intptr_t lp) {
    if (msg == WM_SYSKEYUP && wp == VK_F10) return true;
    bool pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30));  // bit 30: a held key's repeat
    if (!pressed || Capturing()) return false;
    int vk = int(wp);
    bool panel = input::HostKey(input::KeyEvent::SettingsPanel, vk) || (vk == VK_ESCAPE && MenuOpen());
    if (panel) {
        ToggleMenu();
        return true;
    }
    if (input::HostKey(input::KeyEvent::ToggleTextures, vk)) {
        ToggleTextureReplacement();
        return true;
    }
    if (input::HostKey(input::KeyEvent::HideHud, vk)) {
        HudHidden() = !HudHidden();
        return true;
    }
    return false;
}

}  // namespace app
