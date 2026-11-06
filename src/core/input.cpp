// controllers through SDL, keyboard and raw mouse merged into port 0
#include "core/input.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <SDL3/SDL.h>

#include "app/app.h"
#include "app/game_settings.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace input {

namespace {

uint64_t NowMs() {
    auto since = std::chrono::steady_clock::now().time_since_epoch();
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(since).count()) + 1;
}

// Controllers

// a controller takes the lowest free port when it is plugged in and keeps it until it is unplugged
std::mutex g_lock;
bool g_sdl = false;
SDL_Gamepad* g_pads[4] = {};

void AddPad(SDL_JoystickID id) {
    for (SDL_Gamepad* pad : g_pads) {
        if (pad && SDL_GetGamepadID(pad) == id) return;
    }
    SDL_Gamepad** slot = std::find(std::begin(g_pads), std::end(g_pads), nullptr);
    if (slot == std::end(g_pads)) return;
    *slot = SDL_OpenGamepad(id);
    const char* name = *slot ? SDL_GetGamepadName(*slot) : SDL_GetError();
    LOGI("input: controller %d %s: %s", int(slot - g_pads) + 1, *slot ? "plugged in" : "failed to open", name ? name : "");
}

void RemovePad(SDL_JoystickID id) {
    for (int i = 0; i < 4; i++) {
        if (!g_pads[i] || SDL_GetGamepadID(g_pads[i]) != id) continue;
        SDL_CloseGamepad(g_pads[i]);
        g_pads[i] = nullptr;
        LOGI("input: controller %d unplugged", i + 1);
    }
}

// once a millisecond at most, however many ports and threads read
void UpdatePads() {
    static uint64_t last = 0;
    uint64_t now = SDL_GetTicks();
    if (!g_sdl || now == last) return;
    last = now;
    SDL_PumpEvents();
    SDL_Event e;
    while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
        if (e.type == SDL_EVENT_GAMEPAD_ADDED) AddPad(e.gdevice.which);
        else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) RemovePad(e.gdevice.which);
    }
}

// SDL's sticks have y down; not plain negation: -32768 has no positive
int16_t Flip(int value) { return int16_t(-1 - value); }

void ReadPad(SDL_Gamepad* pad, Pad& p) {
    p.connected = true;
    constexpr SDL_GamepadButton kButtons[] = {
        SDL_GAMEPAD_BUTTON_DPAD_UP,    SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_START,     SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK};
    for (int i = 0; i < 8; i++) p.buttons |= uint16_t(SDL_GetGamepadButton(pad, kButtons[i]) << i);
    // A B X Y black white
    constexpr SDL_GamepadButton kAnalog[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                                             SDL_GAMEPAD_BUTTON_WEST,  SDL_GAMEPAD_BUTTON_NORTH,
                                             SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER};
    for (int i = 0; i < 6; i++) p.analog[i] = SDL_GetGamepadButton(pad, kAnalog[i]) ? 0xFF : 0;
    auto axis = [&](SDL_GamepadAxis a) { return int(SDL_GetGamepadAxis(pad, a)); };
    // 0 to 32767 into 0 to 255
    p.analog[6] = uint8_t(std::max(axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), 0) >> 7);
    p.analog[7] = uint8_t(std::max(axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), 0) >> 7);
    p.lx = int16_t(axis(SDL_GAMEPAD_AXIS_LEFTX));
    p.ly = Flip(axis(SDL_GAMEPAD_AXIS_LEFTY));
    int rx = axis(SDL_GAMEPAD_AXIS_RIGHTX);
    p.rx = app::Rdr().invert_pad_x ? Flip(rx) : int16_t(rx);
    p.ry = Flip(axis(SDL_GAMEPAD_AXIS_RIGHTY));
}

// Shared state

std::atomic<bool> g_blocked{false};
std::atomic<int> g_screen{int(Screen::Menu)};
std::atomic<bool> g_aiming{false};
HWND g_hwnd = nullptr;
std::atomic<int> g_dx{0};
std::atomic<int> g_dy{0};

// While the keyboard or mouse was used after the pad's last new press, port 0's controller does not rumble. Only new
// pad presses count, so a drifting stick never takes over from the keyboard.
std::atomic<uint64_t> g_keyboard_ms{0};
std::atomic<uint64_t> g_pad_ms{0};
std::atomic<uint32_t> g_pad_held{0};
std::atomic<bool> g_rumbling{false};  // port 0's motors were last set running

bool KeyboardLast() { return g_keyboard_ms > g_pad_ms; }
bool InPlay() { return Screen(g_screen.load()) == Screen::Play; }
bool Active() { return g_hwnd && GetForegroundWindow() == g_hwnd && !g_blocked; }

// Windows asks for the cursor only when the mouse moves: a menu opening shows it at once
void RefreshCursor() {
    if (g_hwnd) PostMessageW(g_hwnd, WM_SETCURSOR, WPARAM(g_hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

// Bindable actions

// what an action presses: 0-7 analog A B X Y black white LT RT, 8-15 the buttons bits, 16-19 the move keys, 20 and 21
// the right stick up and down; the walk key slows the move keys; host keys press nothing on the pad
constexpr int kAnalogA = 0;
constexpr int kAnalogB = 1;
constexpr int kAnalogLeftTrigger = 6;
constexpr int kButtonTarget = 8;
constexpr int kDpadUp = 0;
constexpr int kDpadDown = 1;
constexpr int kDpadLeft = 2;
constexpr int kDpadRight = 3;
constexpr int kStart = 4;
constexpr int kMoveTarget = 16;
constexpr int kStickUpTarget = 20;
constexpr int kStickDownTarget = 21;
constexpr int kWalk = -1;
constexpr int kHostKey = -2;
constexpr float kStickMax = 32767.0f;
constexpr float kWalkSpeed = 0.4f;  // of a full stick while the walk key is held

struct Target {
    Action action;
    int button;
    KeyEvent event = KeyEvent::None;
    bool riding = false;  // presses only on a horse, where the on foot actions on its button press nothing
};

// The console pad's layout: A jump and dodge, B melee, X action, Y reload, black special move and zoom, white cycles
// weapons, the D-pad's sides step weapons, LT draws and targets, RT fires, the left thumb crouches and the right one
// starts Dead Eye. A duel draws with the right stick: back grabs, forward draws. On a horse A spurs and X dismounts.
constexpr Target kActions[] = {
    {{"move_forward", "MOVE FORWARD", "W"}, kMoveTarget},
    {{"move_back", "MOVE BACK", "S"}, kMoveTarget + 1},
    {{"strafe_left", "MOVE LEFT", "A"}, kMoveTarget + 2},
    {{"strafe_right", "MOVE RIGHT", "D"}, kMoveTarget + 3},
    {{"walk", "WALK", "Alt"}, kWalk},
    {{"jump", "JUMP / DODGE", "Space"}, kAnalogA},
    {{"crouch", "CROUCH", "C"}, kButtonTarget + 6},
    {{"action", "ACTION / SNEAK", "F"}, 2},
    {{"melee", "MELEE", "E"}, kAnalogB},
    {{"fire", "FIRE", "LMB"}, 7},
    {{"draw", "DRAW / TARGET", "RMB"}, kAnalogLeftTrigger},
    {{"reload", "RELOAD", "R"}, 3},
    {{"dead_eye", "DEAD EYE", "Q,MMB"}, kButtonTarget + 7},
    {{"special", "SPECIAL MOVE / ZOOM", "Z"}, 4},
    {{"cycle_weapons", "CYCLE WEAPONS", "Tab"}, 5},
    {{"next_weapon", "NEXT WEAPON", "Wheel Up"}, kButtonTarget + kDpadRight},
    {{"previous_weapon", "PREVIOUS WEAPON", "Wheel Down"}, kButtonTarget + kDpadLeft},
    {{"duel_grab", "GRAB", "Down"}, kStickDownTarget},
    {{"duel_draw", "DRAW", "Up"}, kStickUpTarget},
    {{"spur", "SPUR", "Space"}, kAnalogA, KeyEvent::None, true},
    {{"dismount", "DISMOUNT", "F"}, 2, KeyEvent::None, true},
    {{"pause", "PAUSE", "Escape"}, kButtonTarget + kStart},
    {{"back", "BACK", "Backspace"}, kButtonTarget + 5},
    {{"toggle_hud", "TOGGLE HUD", ""}, kHostKey, KeyEvent::HideHud},
    {{"toggle_textures", "TOGGLE TEXTURE REPLACEMENT", ""}, kHostKey, KeyEvent::ToggleTextures},
    {{"settings_panel", "SETTINGS PANEL", "F10"}, kHostKey, KeyEvent::SettingsPanel},
};
constexpr size_t kCount = std::size(kActions);
static_assert(kCount <= 32, "the keyboard activity mask holds an action a bit");

constexpr Section kSections[] = {{0, "ON FOOT"}, {9, "WEAPONS"}, {17, "DUEL"}, {19, "RIDING"}, {21, "GENERAL"}};

// on a horse an action presses when it is a riding one or no riding one shares its button
std::atomic<bool> g_riding{false};

bool Pressable(size_t action, bool riding) {
    if (kActions[action].riding) return riding;
    if (!riding) return true;
    return std::none_of(std::begin(kActions), std::end(kActions),
                        [&](const Target& t) { return t.riding && t.button == kActions[action].button; });
}

// Key names

// the wheel binds as two keys, codes Windows leaves unassigned; a notch holds the actions bound to it for one tap
constexpr int kWheelUp = 0x0E;
constexpr int kWheelDown = 0x0F;
constexpr uint64_t kWheelTapMs = 80;
std::atomic<uint64_t> g_wheel_ms[2] = {};  // the last notch up, down

struct Name {
    int vk;
    const char* name;
};
constexpr Name kNames[] = {
    {VK_LBUTTON, "LMB"}, {VK_RBUTTON, "RMB"}, {VK_MBUTTON, "MMB"}, {VK_XBUTTON1, "Mouse4"}, {VK_XBUTTON2, "Mouse5"},
    {kWheelUp, "Wheel Up"}, {kWheelDown, "Wheel Down"},
    {VK_SPACE, "Space"}, {VK_RETURN, "Enter"}, {VK_ESCAPE, "Escape"}, {VK_BACK, "Backspace"}, {VK_TAB, "Tab"},
    {VK_SHIFT, "Shift"}, {VK_CONTROL, "Ctrl"}, {VK_MENU, "Alt"}, {VK_CAPITAL, "CapsLock"},
    {VK_UP, "Up"}, {VK_DOWN, "Down"}, {VK_LEFT, "Left"}, {VK_RIGHT, "Right"},
    {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"}, {VK_HOME, "Home"}, {VK_END, "End"},
    {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"},
    {'A', "A"}, {'B', "B"}, {'C', "C"}, {'D', "D"}, {'E', "E"}, {'F', "F"}, {'G', "G"}, {'H', "H"}, {'I', "I"},
    {'J', "J"}, {'K', "K"}, {'L', "L"}, {'M', "M"}, {'N', "N"}, {'O', "O"}, {'P', "P"}, {'Q', "Q"}, {'R', "R"},
    {'S', "S"}, {'T', "T"}, {'U', "U"}, {'V', "V"}, {'W', "W"}, {'X', "X"}, {'Y', "Y"}, {'Z', "Z"},
    {'0', "0"}, {'1', "1"}, {'2', "2"}, {'3', "3"}, {'4', "4"}, {'5', "5"}, {'6', "6"}, {'7', "7"}, {'8', "8"},
    {'9', "9"},
    {VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F4, "F4"}, {VK_F5, "F5"}, {VK_F6, "F6"}, {VK_F7, "F7"},
    {VK_F8, "F8"}, {VK_F9, "F9"}, {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},
    {VK_NUMPAD0, "Numpad0"}, {VK_NUMPAD1, "Numpad1"}, {VK_NUMPAD2, "Numpad2"}, {VK_NUMPAD3, "Numpad3"},
    {VK_NUMPAD4, "Numpad4"}, {VK_NUMPAD5, "Numpad5"}, {VK_NUMPAD6, "Numpad6"}, {VK_NUMPAD7, "Numpad7"},
    {VK_NUMPAD8, "Numpad8"}, {VK_NUMPAD9, "Numpad9"}, {VK_ADD, "NumpadPlus"}, {VK_SUBTRACT, "NumpadMinus"},
    {VK_MULTIPLY, "NumpadStar"}, {VK_DIVIDE, "NumpadSlash"}, {VK_DECIMAL, "NumpadPeriod"},
    {VK_OEM_3, "Backtick"}, {VK_OEM_MINUS, "Minus"}, {VK_OEM_PLUS, "Equals"}, {VK_OEM_4, "LBracket"},
    {VK_OEM_6, "RBracket"}, {VK_OEM_5, "Backslash"}, {VK_OEM_1, "Semicolon"}, {VK_OEM_7, "Quote"},
    {VK_OEM_COMMA, "Comma"}, {VK_OEM_PERIOD, "Period"}, {VK_OEM_2, "Slash"},
};

// US layout scan codes of the keys that move with the layout: a bind names the US key at that position
struct Position {
    int vk;
    UINT scan;
};
constexpr Position kPositions[] = {
    {'1', 0x02}, {'2', 0x03}, {'3', 0x04}, {'4', 0x05}, {'5', 0x06}, {'6', 0x07}, {'7', 0x08}, {'8', 0x09},
    {'9', 0x0A}, {'0', 0x0B}, {VK_OEM_MINUS, 0x0C}, {VK_OEM_PLUS, 0x0D},
    {'Q', 0x10}, {'W', 0x11}, {'E', 0x12}, {'R', 0x13}, {'T', 0x14}, {'Y', 0x15}, {'U', 0x16}, {'I', 0x17},
    {'O', 0x18}, {'P', 0x19}, {VK_OEM_4, 0x1A}, {VK_OEM_6, 0x1B},
    {'A', 0x1E}, {'S', 0x1F}, {'D', 0x20}, {'F', 0x21}, {'G', 0x22}, {'H', 0x23}, {'J', 0x24}, {'K', 0x25},
    {'L', 0x26}, {VK_OEM_1, 0x27}, {VK_OEM_7, 0x28}, {VK_OEM_3, 0x29}, {VK_OEM_5, 0x2B},
    {'Z', 0x2C}, {'X', 0x2D}, {'C', 0x2E}, {'V', 0x2F}, {'B', 0x30}, {'N', 0x31}, {'M', 0x32},
    {VK_OEM_COMMA, 0x33}, {VK_OEM_PERIOD, 0x34}, {VK_OEM_2, 0x35},
};

std::mutex g_bind_lock;
std::array<std::vector<int>, kCount> g_binds;

int Vk(const std::string& name) {
    for (const Name& n : kNames) {
        if (_stricmp(n.name, name.c_str()) == 0) return n.vk;
    }
    return 0;
}

const char* NameOf(int vk) {
    for (const Name& n : kNames) {
        if (n.vk == vk) return n.name;
    }
    return nullptr;
}

UINT ScanOf(int vk) {
    for (const Position& p : kPositions) {
        if (p.vk == vk) return p.scan;
    }
    return 0;
}

// the keyboard layout the player types with, switched per window thread
HKL Layout() { return GetKeyboardLayout(GetWindowThreadProcessId(g_hwnd, nullptr)); }

// the key the current layout has at the bind's position
int LiveVk(int vk) {
    UINT scan = ScanOf(vk);
    if (!scan) return vk;
    UINT live = MapVirtualKeyExW(scan, MAPVK_VSC_TO_VK_EX, Layout());
    return live ? int(live) : vk;
}

// a key the window was sent, as the position bound to it
int PositionVk(int live) {
    if (NameOf(live) && !ScanOf(live)) return live;
    UINT scan = MapVirtualKeyExW(UINT(live), MAPVK_VK_TO_VSC, Layout());
    for (const Position& p : kPositions) {
        if (p.scan == scan) return p.vk;
    }
    return live;
}

// the key as the player's keyboard labels it
std::string Label(int vk) {
    int live = LiveVk(vk);
    if (live == vk) return NameOf(vk);
    bool alnum = (live >= 'A' && live <= 'Z') || (live >= '0' && live <= '9');
    if (alnum) return std::string(1, char(live));
    // the top bit flags a dead key
    UINT c = MapVirtualKeyExW(UINT(live), MAPVK_VK_TO_CHAR, Layout()) & 0x7FFF;
    // a comma would split the key list
    if (c == ',') return NameOf(VK_OEM_COMMA);
    if (c > ' ' && c < 0x7F) return std::string(1, char(c));
    return NameOf(vk);
}

std::vector<int> Parse(const std::string& list) {
    std::vector<int> out;
    size_t start = 0;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        if (comma == std::string::npos) comma = list.size();
        if (int vk = Vk(list.substr(start, comma - start))) out.push_back(vk);
        start = comma + 1;
    }
    return out;
}

// the saved names, or the player's labels for the panel
std::string Join(const std::vector<int>& vks, bool labels = false) {
    std::string s;
    for (int vk : vks) {
        if (!s.empty()) s += ',';
        s += labels ? Label(vk) : NameOf(vk);
    }
    return s;
}

bool Mouse(int vk) {
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2 ||
           vk == kWheelUp || vk == kWheelDown;
}

bool Down(int vk) {
    if (vk == kWheelUp || vk == kWheelDown) return NowMs() - g_wheel_ms[vk == kWheelDown] < kWheelTapMs;
    return GetAsyncKeyState(LiveVk(vk)) < 0;
}

// vk as the window sends it, on the player's layout
bool Bound(KeyEvent event, int vk) {
    vk = PositionVk(vk);
    std::lock_guard lock(g_bind_lock);
    for (size_t i = 0; i < kCount; i++) {
        if (kActions[i].event != event) continue;
        if (std::find(g_binds[i].begin(), g_binds[i].end(), vk) != g_binds[i].end()) return true;
    }
    return false;
}

// Keyboard into the pad

// Keys held while the game had no focus (the focusing click, Escape closing the panel) wait for release. A click that
// opened or closed a menu must not reach the game: the mouse buttons wait for release.
std::atomic<bool> g_settle{false};
std::atomic<bool> g_settle_mouse{false};
std::atomic<uint32_t> g_keys_held{0};

bool AnyKeyDown(const std::array<std::vector<int>, kCount>& binds) {
    for (const auto& keys : binds) {
        if (std::any_of(keys.begin(), keys.end(), Down)) return true;
    }
    return Down(VK_RETURN) || Down(VK_BACK) || Down(VK_ESCAPE);
}

void UpdateMouseSettle() {
    if (!g_settle_mouse) return;
    bool held = Down(VK_LBUTTON) || Down(VK_RBUTTON) || Down(VK_MBUTTON) || Down(VK_XBUTTON1) || Down(VK_XBUTTON2);
    if (!held) g_settle_mouse = false;
}

// out of play the mouse buttons and the wheel work the menus, so they press no action there
bool ActionHeld(const std::vector<int>& keys, bool menu) {
    bool no_mouse = menu || g_settle_mouse;
    return std::any_of(keys.begin(), keys.end(), [&](int vk) { return Down(vk) && !(no_mouse && Mouse(vk)); });
}

// in a menu the pause key backs out as B; the pause menu, which Start closes, and cutscenes, which Start skips, keep
// it on Start
int RemapTarget(int b) {
    bool back = Screen(g_screen.load()) == Screen::Menu && b == kButtonTarget + kStart;
    return back ? kAnalogB : b;
}

// what the keys push the pad's sticks
struct Sticks {
    int move[4] = {};  // forward, back, left, right
    int look = 0;      // the right stick, up positive
    bool walk = false;
    bool aiming = false;  // on foot: the game moves forward and sideways by each stick axis on its own (sub_00149F40)
};

void PressTarget(int b, Pad& p, Sticks& s) {
    if (b == kWalk) s.walk = true;
    else if (b == kStickUpTarget) s.look++;
    else if (b == kStickDownTarget) s.look--;
    else if (b >= kMoveTarget) s.move[b - kMoveTarget] = 1;
    else if (b < kButtonTarget) p.analog[b] = 0xFF;
    else p.buttons |= uint16_t(1u << (b - kButtonTarget));
}

// Out of play Enter and a left click confirm (a click skips a movie too), Backspace and the right button back out,
// the arrows and the wheel move the selection, whatever the keys are bound to
void PressMenuKeys(Pad& p) {
    if (InPlay()) return;
    bool click = !g_settle_mouse;
    if (Down(VK_RETURN) || (click && Down(VK_LBUTTON))) p.analog[kAnalogA] = 0xFF;
    if (Down(VK_BACK) || (click && Down(VK_RBUTTON))) p.analog[kAnalogB] = 0xFF;
    constexpr std::pair<int, int> kKeys[] = {{VK_UP, kDpadUp},       {VK_DOWN, kDpadDown},     {VK_LEFT, kDpadLeft},
                                             {VK_RIGHT, kDpadRight}, {kWheelUp, kDpadUp},      {kWheelDown, kDpadDown}};
    for (auto [vk, bit] : kKeys) {
        if (Down(vk)) p.buttons |= uint16_t(1u << bit);
    }
}

// With the aim toggle on, in play a press of the draw key draws and holds until the next press
bool g_aim_latched = false;
bool g_aim_key_was = false;

bool ToggledAim(bool held) {
    bool press = held && !g_aim_key_was;
    g_aim_key_was = held;
    if (!app::Rdr().aim_toggle || !InPlay()) {
        g_aim_latched = false;
        return held;
    }
    if (press) g_aim_latched = !g_aim_latched;
    return g_aim_latched;
}

void NoteKeysHeld(uint32_t keys) {
    if (keys & ~g_keys_held.exchange(keys)) g_keyboard_ms = NowMs();
}

// the move keys as the left stick, the walk key slowing them; the duel's draw keys as the right stick
void ApplySticks(Pad& p, const Sticks& s) {
    int x = s.move[3] - s.move[2];
    int y = s.move[0] - s.move[1];
    float v = s.walk ? kWalkSpeed : 1.0f;
    // a diagonal keeps the stick's length, aiming each axis stays full
    if (x && y && !s.aiming) v *= 0.70710678f;
    if (x) p.lx = int16_t(float(x) * v * kStickMax);
    if (y) p.ly = int16_t(float(y) * v * kStickMax);
    if (s.look) p.ry = int16_t(std::clamp(s.look, -1, 1) * int(kStickMax));
}

void Keyboard(Pad& p) {
    std::array<std::vector<int>, kCount> binds;
    {
        std::lock_guard lock(g_bind_lock);
        binds = g_binds;
    }
    if (g_settle) {
        if (AnyKeyDown(binds)) return;
        g_settle = false;
    }
    UpdateMouseSettle();
    bool menu = !InPlay();
    bool riding = g_riding && !menu;
    Sticks sticks;
    sticks.aiming = g_aiming && !riding;
    uint32_t keys = 0;
    for (size_t i = 0; i < kCount; i++) {
        if (kActions[i].button == kHostKey || !Pressable(i, riding)) continue;
        bool held = ActionHeld(binds[i], menu);
        if (kActions[i].button == kAnalogLeftTrigger) held = ToggledAim(held);
        if (!held) continue;
        keys |= 1u << i;
        PressTarget(RemapTarget(kActions[i].button), p, sticks);
    }
    PressMenuKeys(p);
    NoteKeysHeld(keys);
    ApplySticks(p, sticks);
}

// the cursor stays inside the window in play
void Clip() {
    RECT r;
    RECT cur;
    GetClientRect(g_hwnd, &r);
    MapWindowPoints(g_hwnd, nullptr, reinterpret_cast<POINT*>(&r), 2);
    if (!GetClipCursor(&cur) || !EqualRect(&r, &cur)) ClipCursor(&r);
}

constexpr int kStickActive = 8000;  // a stick this far off centre counts as held

// the pad on port 0: a new press takes over from the keyboard
void NotePadHeld(const Pad& p) {
    // the mask: buttons bits 0 to 7, analog 8 to 15, the stick axes 16 to 19
    uint32_t held = p.buttons;
    for (int i = 0; i < 8; i++) held |= uint32_t(p.analog[i] > 30) << (8 + i);
    int bit = 16;
    for (int16_t axis : {p.lx, p.ly, p.rx, p.ry}) held |= uint32_t(std::abs(int(axis)) > kStickActive) << bit++;
    if (held & ~g_pad_held.exchange(held)) g_pad_ms = NowMs();
}

void SetMotors(int port, uint16_t left, uint16_t right) {
    std::lock_guard lock(g_lock);
    if (port >= 0 && port < 4 && g_pads[port]) SDL_RumbleGamepad(g_pads[port], left, right, 0);
}

}  // namespace

void Init() {
    if (g_sdl) return;  // started already for the game code window
    // WGI reads the triggers apart and HIDAPI takes PlayStation and Switch pads. Reads come from the guest's threads,
    // so SDL's own thread takes the device notices.
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT, "0");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        LOGE("input: SDL failed to start, no controllers: %s", SDL_GetError());
        return;
    }
    int v = SDL_GetVersion();
    LOGI("input: SDL %d.%d.%d", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
    std::lock_guard lock(g_lock);
    g_sdl = true;
    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; i++) AddPad(ids[i]);
        SDL_free(ids);
    }
}

void StartMouse(void* hwnd) {
    g_hwnd = static_cast<HWND>(hwnd);
    RAWINPUTDEVICE mouse{1, 2, 0, g_hwnd};  // generic desktop page, mouse usage
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
}

Pad Poll(int port) {
    Pad p;
    {
        std::lock_guard lock(g_lock);
        UpdatePads();
        if (SDL_Gamepad* pad = port >= 0 && port < 4 ? g_pads[port] : nullptr) ReadPad(pad, p);
    }
    if (port == 0) {
        if (p.connected) NotePadHeld(p);
        // a motor the game left running when the keyboard took over, the game may not set it again
        if (g_rumbling && KeyboardLast()) {
            SetMotors(0, 0, 0);
            g_rumbling = false;
        }
        p.connected = true;
        // the 250 ms port poller is no game update: only the game's threads press keys
        if (Active() && kernel::Current()) {
            Keyboard(p);
        }
    }
    if (g_blocked) p = Pad{.connected = p.connected};
    if (app::kGame.pad) app::kGame.pad(port, p);
    return p;
}

uint16_t HeldOnAnyController() {
    constexpr std::pair<SDL_GamepadButton, uint16_t> kFaceButtons[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, 0x1000}, {SDL_GAMEPAD_BUTTON_EAST, 0x2000},
        {SDL_GAMEPAD_BUTTON_WEST, 0x4000},  {SDL_GAMEPAD_BUTTON_NORTH, 0x8000}};
    std::lock_guard lock(g_lock);
    UpdatePads();
    uint16_t held = 0;
    for (SDL_Gamepad* pad : g_pads) {
        for (auto [button, mask] : kFaceButtons) {
            if (pad && SDL_GetGamepadButton(pad, button)) held |= mask;
        }
    }
    return held;
}

// the motors run until set again; port 0 is still while the keyboard and mouse were used last
void Rumble(int port, uint16_t left, uint16_t right) {
    if (app::kGame.rumble) app::kGame.rumble(port, left, right);
    if (port == 0 && KeyboardLast()) {
        left = 0;
        right = 0;
    }
    if (port == 0) g_rumbling = left || right;
    uint32_t strength = app::Rdr().vibration_strength;
    left = uint16_t(std::min<uint32_t>(left * strength / 100, 0xFFFF));
    right = uint16_t(std::min<uint32_t>(right * strength / 100, 0xFFFF));
    SetMotors(port, left, right);
}

void SetBlocked(bool blocked) {
    if (g_blocked.exchange(blocked) != blocked) {
        ClipCursor(nullptr);
        g_settle = true;
        RefreshCursor();
    }
}

void OnRawInput(void* handle) {
    RAWINPUT ri;
    UINT size = sizeof(ri);
    UINT read = GetRawInputData(static_cast<HRAWINPUT>(handle), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER));
    if (read == UINT(-1) || ri.header.dwType != RIM_TYPEMOUSE) return;
    const RAWMOUSE& m = ri.data.mouse;
    int notch = !(m.usButtonFlags & RI_MOUSE_WHEEL) ? 0 : SHORT(m.usButtonData) > 0 ? 1 : -1;
    // noted while the input is taken too, so a key capture sees the wheel
    if (notch && g_hwnd && GetForegroundWindow() == g_hwnd) g_wheel_ms[notch < 0] = NowMs();
    if (!Active()) return;
    if (InPlay()) Clip();
    else ClipCursor(nullptr);
    if (m.lLastX || m.lLastY || m.usButtonFlags) g_keyboard_ms = NowMs();
    if (m.usFlags & MOUSE_MOVE_ABSOLUTE) return;
    g_dx += m.lLastX;
    g_dy += m.lLastY;
}

bool CursorShown() { return g_blocked || !InPlay() || GetForegroundWindow() != g_hwnd; }

bool MenuCursorAt(float& x, float& y) {
    POINT p;
    RECT r;
    if (!Active() || InPlay() || !GetCursorPos(&p) || !ScreenToClient(g_hwnd, &p) || !GetClientRect(g_hwnd, &r)) {
        return false;
    }
    if (p.x < 0 || p.y < 0 || p.x >= r.right || p.y >= r.bottom) return false;
    x = float(p.x) / float(r.right);
    y = float(p.y) / float(r.bottom);
    return true;
}

Screen GetScreen() { return Screen(g_screen.load()); }

void SetScreen(Screen screen) {
    if (g_screen.exchange(int(screen)) == int(screen)) return;
    if (screen != Screen::Play) ClipCursor(nullptr);
    g_dx = 0;
    g_dy = 0;
    g_settle_mouse = true;
    RefreshCursor();
}

void SetAiming(bool on) { g_aiming = on; }

void SetRiding(bool on) { g_riding = on; }

bool Aiming() { return g_aiming; }

bool TakeMouse(float& dx, float& dy) {
    int x = g_dx.exchange(0);
    int y = g_dy.exchange(0);
    if ((!x && !y) || !Active() || !InPlay()) return false;
    const app::RdrSettings& s = app::Rdr();
    bool aim = g_aiming;
    dx = float(s.invert_mouse_x ? -x : x) * (aim ? s.aim_horizontal_sensitivity : s.mouse_sensitivity);
    dy = float(s.invert_mouse ? -y : y) * (aim ? s.aim_vertical_sensitivity : s.mouse_vertical_sensitivity);
    return true;
}

bool KeyboardActive() { return g_keyboard_ms >= g_pad_ms; }

const Action* Actions(size_t* count) {
    static const auto list = [] {
        std::array<Action, kCount> a{};
        for (size_t i = 0; i < kCount; i++) a[i] = kActions[i].action;
        return a;
    }();
    *count = kCount;
    return list.data();
}

const Section* Sections(size_t* count) {
    *count = std::size(kSections);
    return kSections;
}

std::string Binding(size_t action) {
    std::lock_guard lock(g_bind_lock);
    return action < kCount ? Join(g_binds[action], true) : std::string();
}

static_assert(kPromptDpadLeft == kButtonTarget + kDpadLeft && kPromptStart == kButtonTarget + kStart);
static_assert(kPromptMoveForward == kMoveTarget && kPromptStickUp == kStickUpTarget && kPromptWalk == kWalk);

std::string PromptKey(PromptButton button) {
    if (!InPlay() && button == kPromptA) return "ENTER";
    if (!InPlay() && button == kPromptB) return "ESC";
    // the front end's Escape backs out as B
    if (GetScreen() == Screen::Menu && button == kPromptStart) return "ENTER";
    bool riding = g_riding && InPlay();
    for (size_t i = 0; i < kCount; i++) {
        if (kActions[i].button != int(button) || !Pressable(i, riding)) continue;
        std::string keys = Binding(i);
        std::string first = keys.substr(0, keys.find(','));
        if (first.empty()) continue;
        for (char& c : first) c = char(std::toupper(uint8_t(c)));
        return first;
    }
    return {};
}

namespace {

// the settings panel with no key would leave a keyboard player no way back into it
bool LockedOut(size_t action) { return kActions[action].event == KeyEvent::SettingsPanel; }

// host keys act on the window's key messages, which mouse buttons do not send
bool Bindable(size_t action, int vk) {
    if (vk && !NameOf(vk)) return false;
    return !(kActions[action].button == kHostKey && Mouse(vk));
}

std::atomic<uint64_t> g_capture_ms{0};  // PressedKey

}  // namespace

void Bind(size_t action, int vk, int slot) {
    if (action >= kCount || slot < 0 || slot > 1 || !Bindable(action, vk)) return;
    {
        std::lock_guard lock(g_bind_lock);
        std::vector<int>& keys = g_binds[action];
        int slots[2] = {keys.size() > 0 ? keys[0] : 0, keys.size() > 1 ? keys[1] : 0};
        if (vk && slots[1 - slot] == vk) slots[1 - slot] = 0;
        slots[slot] = vk;
        if (!slots[0] && !slots[1] && LockedOut(action)) return;
        keys.clear();
        for (int k : slots) {
            if (k) keys.push_back(k);
        }
        app::Rdr().keys[kActions[action].action.id] = Join(keys);
    }
    app::Save();
}

void ResetBindings() {
    app::Rdr().keys.clear();
    LoadBindings();
    app::Save();
}

void LoadBindings() {
    std::lock_guard lock(g_bind_lock);
    const auto& keys = app::Rdr().keys;
    for (size_t i = 0; i < kCount; i++) {
        auto it = keys.find(kActions[i].action.id);
        g_binds[i] = Parse(it != keys.end() ? it->second : kActions[i].action.defaults);
    }
}

int PressedKey() {
    g_capture_ms = NowMs();
    for (const Name& n : kNames) {
        if (Down(n.vk)) return n.vk;
    }
    return 0;
}

// a key capture polled a moment ago: the key is its answer
bool HostKey(KeyEvent event, int vk) { return NowMs() - g_capture_ms > 250 && Bound(event, vk); }

}  // namespace input
