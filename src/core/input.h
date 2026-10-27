#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace input {

// the console controller's report, analog buttons are 0-255
struct Pad {
    bool connected = false;
    uint16_t buttons = 0;  // dpad up/down/left/right, start, back, left thumb, right thumb
    uint8_t analog[8] = {};  // A B X Y black white left trigger right trigger
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

void Init();  // window thread, before boot; the game code window starts it earlier
void StartMouse(void* hwnd);  // raw mouse input for the game window
uint16_t HeldOnAnyController();  // face buttons as XINPUT_GAMEPAD_A/B/X/Y masks, GameInfo::pad left out
// The controller on that port, then GameInfo::pad. The keyboard and mouse merge into port 0, which stays connected.
Pad Poll(int port);
void Rumble(int port, uint16_t left, uint16_t right);
void SetBlocked(bool blocked);  // the settings panel takes the input

// window thread
void OnRawInput(void* handle);
// the cursor shows over menus and the settings panel, in play it is hidden and kept in the window
bool CursorShown();

// What the game is doing, set by the game side each pad update. In play the mouse looks and the keys press their
// actions. Out of play the arrows and wheel, Enter and the left button, Backspace and the right button work the menus
// as the pad's D-pad, A and B; the pause key backs out as B in a menu, the pause menu and cutscenes keep it on Start.
enum class Screen { Play, Menu, Pause, Cutscene };
void SetScreen(Screen screen);
// aiming a weapon: the aim sensitivities apply
void SetAiming(bool on);
bool Aiming();
// raw mouse counts since the last call, scaled by the settings, x right and y down; false when there are none
bool TakeMouse(float& dx, float& dy);
struct LookScale {
    float x;
    float y;
};
// the keyboard or mouse was used after the controller's last new press
bool KeyboardActive();

// keys the host acts on itself, from the window's key messages
enum class KeyEvent { None, HideHud, SettingsPanel, ToggleTextures };

// actions the keyboard and mouse can be bound to, key names comma separated
// letter, digit and symbol keys bind by position as the US key there; Binding shows the player's labels
struct Action {
    const char* id;
    const char* label;
    const char* defaults;
};
// the actions' sections as the settings panel lists them, each from its first action
struct Section {
    size_t first;
    const char* title;
};
const Action* Actions(size_t* count);
const Section* Sections(size_t* count);
std::string Binding(size_t action);
// slot 0 the key, 1 its alternate (the panel's second box); vk 0 clears the slot. A key the other slot has moves.
void Bind(size_t action, int vk, int slot = 0);
void ResetBindings();
void LoadBindings();  // after the settings load
int PressedKey();     // a bindable key or mouse button held now, 0 for none
bool HostKey(KeyEvent event, int vk);  // that event's action is bound to this key and no capture wants it

}  // namespace input
