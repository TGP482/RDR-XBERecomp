// The mouse and gyro on the game side.
// Each pad update the game's state machine says whether the player is in play, a menu, the pause menu or a cutscene.
// In play the game turns every view from swioPad::GetJoystick's right stick, a turn rate a frame: on foot, aiming,
// riding, on a mounted gun. The mouse counts and gyro degrees gathered since the last pad update are added to that
// stick past its dead zone and its cap, so the view turns in proportion to the mouse, through the game's own limits.
#include <cstring>
#include <string_view>

#include "app/game_settings.h"
#include "core/gyro.h"
#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// state machine: the current state, its +4 name; the game loop's running flag, 0 while paused
constexpr uint32_t kCurrentState = 0x6664AC;
constexpr uint32_t kStateName = 0x4;
constexpr uint32_t kGameRunning = 0x40EA24;

// player controller: its swioPad; the aim state (0 aiming, 1 not, 2 starting, 3 stopping)
constexpr uint32_t kPlayerPad = 0x894;
constexpr uint32_t kAimState = 0x600;
// swioPad: +4 the console port it reads, +0x70 the input channels the game takes now, +0x98 a recording plays back
constexpr uint32_t kPadPort = 0x4;
constexpr uint32_t kPadChannels = 0x70;
constexpr uint32_t kPadPlayback = 0x98;

constexpr int kRightStick = 1;
// a full stick turns the aim 0.25 radians a frame at most, so a count turns it at most 0.0015
constexpr float kStickPerCount = 0.006f;
constexpr float kStickPerDegree = 0.42f;

std::string_view StateName() {
    uint32_t state = R32(kCurrentState);
    uint32_t name = state ? R32(state + kStateName) : 0;
    return name ? std::string_view(static_cast<const char*>(HostPtr(name))) : std::string_view();
}

input::Screen CurrentScreen() {
    std::string_view state = StateName();
    bool playing = state == "Game" || state == "Multi";
    if (playing) return G<uint8_t>(kGameRunning) ? input::Screen::Play : input::Screen::Pause;
    bool cinematic = state == "LevelIntro" || state == "LevelOutro" || state == "Attract" || state == "Theater";
    return cinematic ? input::Screen::Cutscene : input::Screen::Menu;
}

// the right stick the mouse and gyro add this frame, in the game's axes: x right, y down
struct Stick {
    float x = 0.0f;
    float y = 0.0f;
};
Stick g_mouse;

// the gyro's yaw is left positive and its pitch down positive
Stick TakeMotion() {
    Stick s;
    float dx;
    float dy;
    if (input::TakeMouse(dx, dy)) {
        s.x = dx * kStickPerCount;
        s.y = dy * kStickPerCount;
    }
    gyro::Turn g = gyro::Take(app::Rdr().gyro);
    s.x -= g.yaw * kStickPerDegree;
    s.y += g.pitch * kStickPerDegree;
    return s;
}

// the player's look running now: its right stick takes the controller's aim sensitivities
bool g_in_look = false;
input::LookScale g_pad_scale{1.0f, 1.0f};

float ReadFloat(uint32_t va) { return GF(va); }

void WriteFloat(uint32_t va, float f) { std::memcpy(HostPtr(va), &f, 4); }

}  // namespace

// ioInput update: the pads, then their listeners
X86_EXTERN_FUNC(__imp__sub_00048390);
X86_FUNC(sub_00048390) {
    input::Screen screen = CurrentScreen();
    input::SetScreen(screen);
    g_mouse = screen == input::Screen::Play ? TakeMotion() : Stick{};
    __imp__sub_00048390(ctx, base);
}

// the player's look, fastcall(player)
X86_EXTERN_FUNC(__imp__sub_00143350);
X86_FUNC(sub_00143350) {
    uint32_t player = ctx.ecx;
    uint32_t aim = R32(player + kAimState);
    bool aiming = aim == 0 || aim == 2;
    uint32_t pad = R32(player + kPlayerPad);
    if (pad && R32(pad + kPadPort) == 0) input::SetAiming(aiming);
    const app::RdrSettings& s = app::Rdr();
    g_pad_scale = aiming ? input::LookScale{s.pad_aim_horizontal_sensitivity, s.pad_aim_vertical_sensitivity}
                         : input::LookScale{1.0f, 1.0f};
    g_in_look = true;
    __imp__sub_00143350(ctx, base);
    g_in_look = false;
}

// swioPad::GetJoystick thiscall(stick, out x y, mode, channels). Port 0's right stick gets this frame's mouse turn;
// in the player's look the controller's share takes the aim sensitivities first.
X86_EXTERN_FUNC(__imp__sub_001A98C0);
X86_FUNC(sub_001A98C0) {
    uint32_t pad = ctx.ecx;
    int stick = int(R32(ctx.esp + 4));
    uint32_t out = R32(ctx.esp + 8);
    uint32_t channels = R32(ctx.esp + 16);
    __imp__sub_001A98C0(ctx, base);
    if (stick != kRightStick || !(R32(pad + kPadChannels) & channels)) return;
    float x = ReadFloat(out);
    float y = ReadFloat(out + 4);
    if (g_in_look) {
        x *= g_pad_scale.x;
        y *= g_pad_scale.y;
    }
    if (R32(pad + kPadPort) == 0 && !G<uint8_t>(pad + kPadPlayback)) {
        x += g_mouse.x;
        y += g_mouse.y;
    }
    WriteFloat(out, x);
    WriteFloat(out + 4, y);
}
