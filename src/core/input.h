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
// The controller F10 > PLAYER 1 to 4 puts on that port, then GameInfo::pad. The keyboard and mouse merge into the pad
// of their port, which stays connected.
Pad Poll(int port);
int KeyboardPort();  // the port the keyboard and mouse are on, -1 for none
int LookPort();      // the port the mouse and gyro turn: the keyboard's, else 0
bool PadConnected(int slot);  // controller slot 0-3
void Rumble(int port, uint16_t left, uint16_t right);
void SetBlocked(bool blocked);  // the settings panel takes the input
void SetLoading(bool loading);  // a loading screen is up: the cursor hides

// window thread
void OnRawInput(void* handle);
// the cursor shows over menus and the settings panel; in play it is hidden and kept in the window, on loading
// screens just hidden
bool CursorShown();
// out of play, the cursor across and down the window from 0 to 1; false outside it, or not in the game's hands
bool MenuCursorAt(float& x, float& y);

// What the game is doing, set by the game side each pad update. In play the mouse looks and the keys press their
// actions. Out of play the arrows and wheel, Enter and the left button, Backspace and the right button work the menus
// as the pad's D-pad, A and B; the pause key backs out as B in a menu, the pause menu and cutscenes keep it on Start.
enum class Screen { Play, Menu, Pause, Cutscene };
void SetScreen(Screen screen);
Screen GetScreen();
// a name being typed, kept on by a call each frame: the keys that type press nothing, Enter and Backspace leave A and
// B alone, and the characters typed meanwhile wait in TakeTyped
void KeepTextEntry();
// a "press start" text showing, kept on by a call each frame: a new Enter press presses Start instead of A
void KeepStartPrompt();
void OnChar(uint32_t ch, bool repeat);  // window thread, WM_CHAR
std::string TakeTyped();
// aiming a weapon: the aim sensitivities apply
void SetAiming(bool on);
bool Aiming();
// on a horse: the RIDING keys take the buttons they share with the on foot ones
void SetRiding(bool on);
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
void LoadBindings();  // after the settings load, the controller's too
int PressedKey();     // a bindable key or mouse button held now, 0 for none
// The controller's buttons by action, on every controller: in play each action's console control is pressed by the
// physical button bound to it, menus keep the stock buttons. A button another action has changes places with the one
// rebound; toggle HUD bound again to its own button, or given -1, comes off it.
const Action* PadActions(size_t* count);
std::string PadBinding(size_t action);
void BindPad(size_t action, int button);
void ResetPadBindings();
// a controller button held now on any controller, -1 for none: 0-7 A B X Y RB LB LT RT, 8-15 the Pad::buttons bits
int PressedPadButton();
// the console controls the game's prompts show, numbered as the actions press them
enum PromptButton {
    kPromptWalk = -1,
    kPromptA = 0,
    kPromptB = 1,
    kPromptX = 2,
    kPromptY = 3,
    kPromptBlack = 4,
    kPromptWhite = 5,
    kPromptLeftTrigger = 6,
    kPromptRightTrigger = 7,
    kPromptDpadLeft = 10,
    kPromptDpadRight = 11,
    kPromptStart = 12,
    kPromptLeftThumb = 14,
    kPromptRightThumb = 15,
    kPromptMoveForward = 16,
    kPromptMoveBack = 17,
    kPromptMoveLeft = 18,
    kPromptMoveRight = 19,
    kPromptStickUp = 20,
    kPromptStickDown = 21,
};
// a prompt's key: in play the first key of the first action pressing the button, out of it Enter and Escape for A and
// B and Enter for the front end's Start; empty when nothing is bound
std::string PromptKey(PromptButton button);
bool HostKey(KeyEvent event, int vk);  // that event's action is bound to this key and no capture wants it

}  // namespace input
