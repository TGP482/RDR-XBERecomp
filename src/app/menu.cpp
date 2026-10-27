// the settings panel on F10 and the FPS counter
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <string>
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
#include "app/menu.h"
#include "app/panel_colors.h"
#include "core/input.h"

namespace app {

bool g_fps_counter = false;

namespace {

std::atomic<bool> g_open{false};

constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"OFF", "2x", "4x", "8x", "16x"};
constexpr uint32_t kFpsCaps[] = {30, 60, 75, 90, 120, 144, 165, 240, 360, kFpsCapRefresh, 0};
// in DisplayMode order
const char* const kDisplayModeNames[] = {"FULLSCREEN", "BORDERLESS", "WINDOWED"};
const char* const kDumpFormatNames[] = {"PNG", "DDS"};

// render resolution in percent of the output
constexpr int kRenderMin = 20;
constexpr int kRenderMax = 200;
constexpr int kRenderStep = 5;

enum class RowId {
    Heading,
    DisplayMode,
    Resolution,
    RenderResolution,
    Vsync,
    MaxFps,
    FpsCounter,
    Aniso,
    TextureReplacement,
    TextureDumping,
    DumpFormat,
};

struct Row {
    RowId id;
    const char* label;
};

constexpr Row kRows[] = {
    {RowId::Heading, "DISPLAY"},
    {RowId::DisplayMode, "DISPLAY MODE"},
    {RowId::Resolution, "RESOLUTION"},
    {RowId::RenderResolution, "RENDER RESOLUTION"},
    {RowId::Vsync, "VERTICAL SYNC"},
    {RowId::MaxFps, "MAX FRAME RATE"},
    {RowId::FpsCounter, "FPS COUNTER"},
    {RowId::Heading, "GRAPHICS"},
    {RowId::Aniso, "ANISOTROPIC FILTERING"},
    {RowId::Heading, "TEXTURES"},
    {RowId::TextureReplacement, "TEXTURE REPLACEMENT"},
    {RowId::TextureDumping, "TEXTURE DUMPING"},
    {RowId::DumpFormat, "DUMP FORMAT"},
};

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

uint32_t StepRenderPercent(uint32_t percent, int step) {
    int count = (kRenderMax - kRenderMin) / kRenderStep + 1;
    int index = (std::clamp(int(percent), kRenderMin, kRenderMax) - kRenderMin) / kRenderStep;
    return uint32_t(kRenderMin + kRenderStep * Wrap(index, step, count));
}

bool SameMode(const Mode& a, const Mode& b) { return a.width == b.width && a.height == b.height; }

// the game window's monitor modes, smallest first so Left steps down
std::vector<Mode> ReadModes() {
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(Window()), MONITOR_DEFAULTTOPRIMARY), &mi);
    std::vector<Mode> modes;
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(mi.szDevice, i, &dm); i++) {
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

const std::vector<Mode>& Modes() {
    static const std::vector<Mode> modes = ReadModes();
    return modes;
}

Mode ShownMode(const Settings& s) { return s.width && s.height ? Mode{s.width, s.height} : Desktop(); }

void StepResolution(Settings& s, int step) {
    const std::vector<Mode>& modes = Modes();
    if (modes.empty()) return;
    Mode now = ShownMode(s);
    int index = 0;
    for (size_t i = 0; i < modes.size(); i++) {
        if (SameMode(modes[i], now)) index = int(i);
    }
    Mode m = modes[size_t(Wrap(index, step, int(modes.size())))];
    // the desktop's own mode is kept as 0, which follows the desktop
    bool native = SameMode(m, Desktop());
    s.width = native ? 0 : m.width;
    s.height = native ? 0 : m.height;
}

// the setting an ON / OFF row flips, nullptr for any other row
bool* ToggleFlag(Settings& s, RowId id) {
    switch (id) {
        case RowId::Vsync: return &s.vsync;
        case RowId::FpsCounter: return &g_fps_counter;
        case RowId::TextureReplacement: return &s.custom_textures;
        case RowId::TextureDumping: return &s.dump_textures;
        default: return nullptr;
    }
}

void Step(RowId id, int step, Settings& s) {
    if (bool* flag = ToggleFlag(s, id)) {
        *flag = !*flag;
        return;
    }
    switch (id) {
        case RowId::DisplayMode: s.display_mode = DisplayMode(Wrap(int(s.display_mode), step, 3)); break;
        case RowId::Resolution: StepResolution(s, step); break;
        case RowId::RenderResolution: s.render_percent = StepRenderPercent(s.render_percent, step); break;
        case RowId::MaxFps: StepTable(kFpsCaps, s.fps_cap, step); break;
        case RowId::Aniso: StepTable(kAnisoValues, s.aniso, step); break;
        case RowId::DumpFormat: s.texture_dump_format = uint32_t(Wrap(int(s.texture_dump_format), step, 2)); break;
        default: break;
    }
}

std::string ModeText(const Mode& m) { return std::to_string(m.width) + "x" + std::to_string(m.height); }

std::string FpsCapText(uint32_t cap) {
    if (cap == kFpsCapRefresh) return "REFRESH RATE";
    return cap ? std::to_string(cap) : "UNLIMITED";
}

std::string RenderText(const Settings& s) {
    Mode out = ShownMode(s);
    Mode render{out.width * s.render_percent / 100, out.height * s.render_percent / 100};
    return std::to_string(s.render_percent) + "% (" + ModeText(render) + ")";
}

std::string Text(RowId id, Settings& s) {
    if (bool* flag = ToggleFlag(s, id)) return *flag ? "ON" : "OFF";
    switch (id) {
        case RowId::DisplayMode: return kDisplayModeNames[std::min(uint32_t(s.display_mode), 2u)];
        case RowId::Resolution: return ModeText(ShownMode(s));
        case RowId::RenderResolution: return RenderText(s);
        case RowId::MaxFps: return FpsCapText(kFpsCaps[NearestIndex(kFpsCaps, s.fps_cap)]);
        case RowId::Aniso: return kAnisoNames[NearestIndex(kAnisoValues, s.aniso)];
        case RowId::DumpFormat: return kDumpFormatNames[std::min(s.texture_dump_format, 1u)];
        default: return "";
    }
}

// Changes

// display mode and resolution wait for Apply, as Windows' display settings do; closing drops them
std::atomic<bool> g_display_held{false};
Settings g_held;

bool DisplayRow(RowId id) { return id == RowId::DisplayMode || id == RowId::Resolution; }

void HoldDisplay(RowId id, int step) {
    const Settings& s = Get();
    if (!g_display_held) {
        g_held.display_mode = s.display_mode;
        g_held.width = s.width;
        g_held.height = s.height;
    }
    Step(id, step, g_held);
    g_display_held = g_held.display_mode != s.display_mode || g_held.width != s.width || g_held.height != s.height;
}

void ApplyHeld() {
    Settings& s = Get();
    s.display_mode = g_held.display_mode;
    s.width = g_held.width;
    s.height = g_held.height;
    g_display_held = false;
    ApplyDisplay();
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

void RestoreDefaults() {
    std::string join_ip = Get().join_ip;  // no row on the panel
    Get() = Settings{};
    Get().join_ip = join_ip;
    g_fps_counter = false;
    g_display_held = false;
    ApplyDisplay();
    Save();
}

// shown dimmed and left as it is
bool Greyed(RowId id) { return id == RowId::DumpFormat && !Get().dump_textures; }

// Drawing

constexpr float kValueEms = 12.0f;  // the value column's width

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

void Section(const char* title) {
    float em = ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(0, em * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(title);
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
int ValueBlock(const RowBox& box, const char* value) {
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
    ImGui::SetCursorPos(ImVec2(box.row_x + ImGui::GetStyle().ItemInnerSpacing.x, box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (int clicked = ValueBlock(box, value)) step = clicked;
    ImGui::PopID();
    // the cursor back to the row's start, then past the whole row
    ImGui::SetCursorPos(ImVec2(box.row_x, box.row_y));
    ImGui::Dummy(ImVec2(box.row_w, box.row_h));
    return step;
}

void DrawRows() {
    for (const Row& r : kRows) {
        if (r.id == RowId::Heading) {
            Section(r.label);
            continue;
        }
        bool held = g_display_held && DisplayRow(r.id);
        std::string value = Text(r.id, held ? g_held : Get());
        bool greyed = Greyed(r.id);
        ImGui::BeginDisabled(greyed);
        int step = OptionRow(r.label, value.c_str());
        ImGui::EndDisabled();
        if (step && !greyed) ChangeRow(r.id, step);
    }
}

// Back, Defaults and Apply on the right
void DrawButtons() {
    float bw = ImGui::GetFontSize() * 8;
    float pad = ImGui::GetContentRegionAvail().x - bw * 3 - ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(pad, 0.0f));
    if (ImGui::Button("Back", ImVec2(bw, 0))) ToggleMenu();
    ImGui::SameLine();
    if (ImGui::Button("Defaults", ImVec2(bw, 0))) RestoreDefaults();
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_display_held);
    if (ImGui::Button("Apply", ImVec2(bw, 0))) ApplyHeld();
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
        DrawTitle("RECOMP SETTINGS");
        float footer = ImGui::GetFrameHeightWithSpacing() + em;
        ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
        DrawRows();
        ImGui::EndChild();
        ImGui::Separator();
        DrawButtons();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(int(std::size(kPanelColors)));
}

// top left, white with a black shadow
void DrawFps(double fps) {
    char text[32];
    std::snprintf(text, sizeof(text), "FPS: %.1f", fps);
    float size = ImGui::GetFontSize();
    ImVec2 at(size * 0.5f, size * 0.5f);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddText(ImGui::GetFont(), size, ImVec2(at.x + 1, at.y + 1), IM_COL32_BLACK, text);
    draw->AddText(ImGui::GetFont(), size, at, IM_COL32_WHITE, text);
}

}  // namespace

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

void ToggleMenu() {
    g_open.store(!MenuOpen(), std::memory_order_relaxed);
    g_display_held = false;
}

void DrawUi(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    io.KeyRepeatRate = 0.1f;  // held arrows step ten times a second, ImGui's twenty is too fast for settings
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    if (g_fps_counter) DrawFps(fps);
    if (MenuOpen()) DrawPanel();
}

// F10 opens and closes, Escape closes; F10 is a system key, so Windows' menu key handling is kept away from it
bool MenuMessage(void*, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    if (msg == WM_SYSKEYUP && wp == VK_F10) {
        *result = 0;
        return true;
    }
    bool pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30));  // bit 30: a held key's repeat
    bool toggle = wp == VK_F10 || (wp == VK_ESCAPE && MenuOpen());
    if (!pressed || !toggle) return false;
    ToggleMenu();
    *result = 0;
    return true;
}

// the game gets no controller input while the panel is open
void MenuPad(int, input::Pad& pad) {
    if (MenuOpen()) pad = input::Pad{.connected = pad.connected};
}

}  // namespace app
