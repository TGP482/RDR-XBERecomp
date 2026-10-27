// The mouse and gyro on the game side.
// Each pad update the game's state machine says whether the player is in play, a menu, the pause menu or a cutscene.
// Aiming, the mouse and gyro turn the view by an angle: the player's heading turns by the yaw and the aim's pitch
// moves by the pitch, through the game's own limits, so a count is always the same angle. Out of aim, in the
// camera, riding or on a mounted gun, they are added to swioPad::GetJoystick's right stick past its dead zone.
#include <cmath>
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
// player controller look: the heading the look turns, the flick boost of its turn rate, the two headings it turns
constexpr uint32_t kLookHeading = 0xA0C;
constexpr uint32_t kLookBoost = 0x6DC;
constexpr uint32_t kBodyHeading = 0x8EC;
constexpr uint32_t kCoverHeading = 0x91C;
// player controller +0xC: its parts, +0x74 the mover, +0x38 its state, turning only while 0
constexpr uint32_t kPlayerParts = 0xC;
constexpr uint32_t kPartsMover = 0x74;
constexpr uint32_t kMoverState = 0x38;
// the look's turn rate a frame for a full stick: the base (the options' sensitivity) up to the most by the boost
constexpr uint32_t kAimRateMost = 0x413654;
constexpr uint32_t kAimRateBase = 0x681BA8;
constexpr uint32_t kLookRateMost = 0x4136B4;
constexpr uint32_t kLookRateBase = 0x681BB4;
constexpr uint32_t kBoostScale = 0x413650;
constexpr uint32_t kBoostScaleFine = 0x3E4248;
// swioPad: +4 the console port it reads, +0x70 the input channels the game takes now, +0x98 a recording plays back
constexpr uint32_t kPadPort = 0x4;
constexpr uint32_t kPadChannels = 0x70;
constexpr uint32_t kPadPlayback = 0x98;

constexpr int kRightStick = 1;
constexpr float kRadiansPerCount = 0.0015f;
constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
// out of aim: a full stick turns the view 0.2 radians a frame at most
constexpr float kStickPerCount = 0.006f;
constexpr float kStickPerDegree = 0.42f;
constexpr float kLeastRate = 0.0001f;

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

// the mouse and gyro this pad update: as a right stick in the game's axes (x right, y down), and as radians
struct Stick {
    float x = 0.0f;
    float y = 0.0f;
};
struct Motion {
    Stick stick;
    Stick turn;
};
Motion g_motion;

// the gyro's yaw is left positive and its pitch down positive
Motion TakeMotion() {
    Motion m;
    float dx;
    float dy;
    if (input::TakeMouse(dx, dy)) {
        m.stick = {dx * kStickPerCount, dy * kStickPerCount};
        m.turn = {dx * kRadiansPerCount, dy * kRadiansPerCount};
    }
    gyro::Turn g = gyro::Take(app::Rdr().gyro);
    m.stick.x -= g.yaw * kStickPerDegree;
    m.stick.y += g.pitch * kStickPerDegree;
    m.turn.x -= g.yaw * kRadiansPerDegree;
    m.turn.y += g.pitch * kRadiansPerDegree;
    return m;
}

// port 0's player controller, from its last look; aiming, its turn goes to the look rather than the stick
uint32_t g_player = 0;
bool g_turns_look = false;

bool Aiming(uint32_t player) {
    uint32_t aim = R32(player + kAimState);
    return aim == 0 || aim == 2;
}

bool PortZero(uint32_t player) {
    uint32_t pad = R32(player + kPlayerPad);
    return pad && R32(pad + kPadPort) == 0;
}

float ReadFloat(uint32_t va) { return GF(va); }

void WriteFloat(uint32_t va, float f) { std::memcpy(HostPtr(va), &f, 4); }

// radians a frame the look turns for a full stick
float LookRate(uint32_t player, bool aiming) {
    float most = ReadFloat(aiming ? kAimRateMost : kLookRateMost);
    float base = ReadFloat(aiming ? kAimRateBase : kLookRateBase);
    float boost = ReadFloat(player + kLookBoost) * ReadFloat(kBoostScale) * ReadFloat(kBoostScaleFine);
    return (most - base) * boost + base;
}

// the heading the look turns while the player stands, as the game's own aim turn does; 0 for none
uint32_t TurnableHeading(uint32_t player) {
    uint32_t parts = R32(player + kPlayerParts);
    uint32_t mover = parts ? R32(parts + kPartsMover) : 0;
    if (!mover || R32(mover + kMoverState) != 0) return 0;
    uint32_t heading = R32(player + kLookHeading);
    bool turnable = heading == player + kBodyHeading || heading == player + kCoverHeading;
    return turnable ? heading : 0;
}

// a 3x3 by rows times a turn about the up axis, as the game turns a heading (0x4B8D0); negative turns right
void TurnHeading(uint32_t heading, float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    for (uint32_t row = heading; row < heading + 36; row += 12) {
        float x = ReadFloat(row);
        float z = ReadFloat(row + 8);
        WriteFloat(row, x * c + z * s);
        WriteFloat(row + 8, z * c - x * s);
    }
}

// the aim's stick: the controller's share by the aim sensitivities, then the turn as the stick the look's rate makes
// it: the yaw turns the heading when there is one, else the aim
void AddTurn(uint32_t player, uint32_t stick) {
    const app::RdrSettings& s = app::Rdr();
    float x = ReadFloat(stick) * s.pad_aim_horizontal_sensitivity;
    float y = ReadFloat(stick + 4) * s.pad_aim_vertical_sensitivity;
    float rate = LookRate(player, true);
    if (g_turns_look && rate > kLeastRate) {
        uint32_t heading = TurnableHeading(player);
        if (heading) TurnHeading(heading, -g_motion.turn.x);
        else x += g_motion.turn.x / rate;
        y += g_motion.turn.y / rate;
    }
    WriteFloat(stick, x);
    WriteFloat(stick + 4, y);
}

}  // namespace

// ioInput update: the pads, then their listeners
X86_EXTERN_FUNC(__imp__sub_00048390);
X86_FUNC(sub_00048390) {
    input::Screen screen = CurrentScreen();
    input::SetScreen(screen);
    bool play = screen == input::Screen::Play;
    if (!play) g_player = 0;
    g_motion = play ? TakeMotion() : Motion{};
    g_turns_look = g_player && Aiming(g_player);
    __imp__sub_00048390(ctx, base);
}

// the player's look, thiscall(player, right stick) ret 4
X86_EXTERN_FUNC(__imp__sub_00143350);
X86_FUNC(sub_00143350) {
    uint32_t player = ctx.ecx;
    uint32_t stick = R32(ctx.esp + 4);
    bool aiming = Aiming(player);
    if (PortZero(player)) {
        g_player = player;
        input::SetAiming(aiming);
        if (aiming) AddTurn(player, stick);
    }
    __imp__sub_00143350(ctx, base);
}

// swioPad::GetJoystick thiscall(stick, out x y, mode, channels): out of aim, port 0's right stick gets this pad
// update's mouse and gyro
X86_EXTERN_FUNC(__imp__sub_001A98C0);
X86_FUNC(sub_001A98C0) {
    uint32_t pad = ctx.ecx;
    int stick = int(R32(ctx.esp + 4));
    uint32_t out = R32(ctx.esp + 8);
    uint32_t channels = R32(ctx.esp + 16);
    __imp__sub_001A98C0(ctx, base);
    if (g_turns_look || stick != kRightStick || !(R32(pad + kPadChannels) & channels)) return;
    if (R32(pad + kPadPort) != 0 || G<uint8_t>(pad + kPadPlayback)) return;
    WriteFloat(out, ReadFloat(out) + g_motion.stick.x);
    WriteFloat(out + 4, ReadFloat(out + 4) + g_motion.stick.y);
}
