// The mouse, gyro and stick dead zone on the game side.
// Each pad update the game's state machine says whether the player is in play, a menu, the pause menu or a cutscene.
// The mouse and gyro turn the view by an angle, so a count is always the same angle. Aiming, the player's heading
// turns by the yaw and the aim's pitch moves by the pitch, through the game's own limits. In a duel a turn down grabs
// and one up draws, then the turn moves the reticle's yaw and pitch. Out of aim the camera machine's own angles
// turn: the follow camera's bearing and the tilt it reads back from the view, the wagon camera's swing, the mounted
// gun camera's yaw and pitch.
// No aim assist on the mouse, nor on the controller with AIM ASSIST off: aiming finds no target to soft lock onto,
// so nothing pulls the aim, and a soft lock the controller made is let go. Duels and Dead Eye lock their own way and
// stay as they are.
#include <algorithm>
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

// the player's controller; its swioPad; the aim state (0 aiming, 1 not, 2 starting, 3 stopping); on a horse
constexpr uint32_t kPlayer = 0x65EAD8;
constexpr uint32_t kPlayerPad = 0x894;
constexpr uint32_t kAimState = 0x600;
constexpr uint32_t kOnHorse = 0x338;
// player controller: the soft lock's two flags, set only when aiming soft locks a target (sub_00148D10); the game's
// release of the player's target, thiscall(player, hud effect) ret 4
constexpr uint32_t kSoftLockFlags = 0x6A9;
constexpr uint32_t kReleaseTarget = 0x13D5F0;
// player controller look: its yaw and pitch (the duel reticle), the heading the aim turns, the flick boost of its turn
// rate, the two headings it turns
constexpr uint32_t kLookYaw = 0x67C;
constexpr uint32_t kLookPitch = 0x680;
constexpr uint32_t kLookHeading = 0xA0C;
constexpr uint32_t kLookBoost = 0x6DC;
constexpr uint32_t kBodyHeading = 0x8EC;
constexpr uint32_t kCoverHeading = 0x91C;
// player controller +0xC: its parts, +0x74 the mover, +0x38 its state, turning only while 0
constexpr uint32_t kPlayerParts = 0xC;
constexpr uint32_t kPartsMover = 0x74;
constexpr uint32_t kMoverState = 0x38;
// the aim's turn rate a frame for a full stick: the base (the options' sensitivity) up to the most by the boost
constexpr uint32_t kAimRateMost = 0x413654;
constexpr uint32_t kAimRateBase = 0x681BA8;
constexpr uint32_t kBoostScale = 0x413650;
constexpr uint32_t kBoostScaleFine = 0x3E4248;
// swioPad's dead zones from its pad file, a stick reads as centred inside: the left stick, the right
constexpr uint32_t kDeadZoneLeft = 0x4164E0;
constexpr uint32_t kDeadZoneRight = 0x4164E4;
// swioPad: +4 the console port it reads
constexpr uint32_t kPadPort = 0x4;

// player controller +0x88C its camera, +0x70 the camera's rig, +0x9C the rig's camera machine
constexpr uint32_t kPlayerCamera = 0x88C;
constexpr uint32_t kCameraRig = 0x70;
constexpr uint32_t kRigMachine = 0x9C;
// camera machine +0x18 its inputs, +4 their array; an input's value at +8; input 0x15 CameraTilt is the view's tilt
// the player's update measures, up positive, which the follow camera turns from
constexpr uint32_t kMachineInputs = 0x18;
constexpr uint32_t kInputArray = 0x4;
constexpr uint32_t kInputValue = 0x8;
constexpr uint32_t kInputCameraTilt = 0x15;
// camera actions: the wagon pivot's swing about the wagon; the mounted gun's yaw and pitch
constexpr uint32_t kWagonSwing = 0x50;
constexpr uint32_t kGatlingYaw = 0x5C;
constexpr uint32_t kGatlingPitch = 0x60;

constexpr float kRadiansPerCount = 0.0015f;
constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
constexpr float kLeastRate = 0.0001f;
// player controller: the right stick the duel reads (sub_00096750), its y back positive; duel: its state, the
// reticle aimed from 3 on; a mouse turn this far down or up is a full stick
constexpr uint32_t kDuelStick = 0x5E4;
constexpr uint32_t kStickY = 0x4;
constexpr uint32_t kDuelState = 0x0;
constexpr uint32_t kDuelAiming = 3;
constexpr float kDrawFlick = 0.03f;

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

// this pad update's mouse and gyro turn in radians, x right and y down; the first of the aim, the duel or the camera
// to run takes it
struct Turn {
    float x = 0.0f;
    float y = 0.0f;
};
Turn g_turn;

// the gyro's yaw is left positive and its pitch down positive
Turn TakeMotion() {
    Turn t;
    float dx;
    float dy;
    if (input::TakeMouse(dx, dy)) t = {dx * kRadiansPerCount, dy * kRadiansPerCount};
    gyro::Turn g = gyro::Take(app::Rdr().gyro);
    t.x -= g.yaw * kRadiansPerDegree;
    t.y += g.pitch * kRadiansPerDegree;
    return t;
}

Turn TakeTurn() {
    Turn t = g_turn;
    g_turn = {};
    return t;
}

bool Aiming(uint32_t player) {
    uint32_t aim = R32(player + kAimState);
    return aim == 0 || aim == 2;
}

bool PortZero(uint32_t player) {
    uint32_t pad = R32(player + kPlayerPad);
    return pad && R32(pad + kPadPort) == 0;
}

// the player's aim may not soft lock: the keyboard and mouse were used last, or AIM ASSIST is off
bool NoAimAssist(uint32_t player) {
    return !app::Rdr().aim_assist || (PortZero(player) && input::KeyboardActive());
}

// lets go of a soft lock once it may not hold, as the targeting update does
void DropSoftLock(X86Context& ctx, uint32_t player) {
    if (!G<uint8_t>(player + kSoftLockFlags) || !NoAimAssist(player)) return;
    W16(player + kSoftLockFlags, 0);
    ctx.ecx = player;
    CallGuest(ctx, kReleaseTarget, {0});
    ctx.ecx = player;
}

float ReadFloat(uint32_t va) { return GF(va); }

void WriteFloat(uint32_t va, float f) { std::memcpy(HostPtr(va), &f, 4); }

void AddFloat(uint32_t va, float f) { WriteFloat(va, ReadFloat(va) + f); }

// radians a frame the aim turns for a full stick
float AimRate(uint32_t player) {
    float most = ReadFloat(kAimRateMost);
    float base = ReadFloat(kAimRateBase);
    float boost = ReadFloat(player + kLookBoost) * ReadFloat(kBoostScale) * ReadFloat(kBoostScaleFine);
    return (most - base) * boost + base;
}

// the heading the aim turns while the player stands, as the game's own aim turn does; 0 for none
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

// the aim's stick: the controller's share by the aim sensitivities, then the turn as the stick the aim's rate makes
// it: the yaw turns the heading when there is one, else the aim
void AddAimTurn(uint32_t player, uint32_t stick, Turn t) {
    const app::RdrSettings& s = app::Rdr();
    float x = ReadFloat(stick) * s.pad_aim_horizontal_sensitivity;
    float y = ReadFloat(stick + 4) * s.pad_aim_vertical_sensitivity;
    float rate = AimRate(player);
    if (rate > kLeastRate) {
        uint32_t heading = TurnableHeading(player);
        if (heading) TurnHeading(heading, -t.x);
        else x += t.x / rate;
        y += t.y / rate;
    }
    WriteFloat(stick, x);
    WriteFloat(stick + 4, y);
}

// Each pad update the setting goes into the game's dead zones, the setting's 0 puts the game's own back. A value there
// that is not the last one written is the pad file's.
struct DeadZone {
    float game = 0.0f;
    float written = -1.0f;
};
DeadZone g_left_zone;
DeadZone g_right_zone;

void UpdateDeadZone(uint32_t va, DeadZone& zone) {
    float now = ReadFloat(va);
    if (now != zone.written) zone.game = now;
    float setting = app::Rdr().pad_dead_zone;
    zone.written = setting > 0.0f ? setting : zone.game;
    WriteFloat(va, zone.written);
}

void UpdateDeadZones() {
    UpdateDeadZone(kDeadZoneLeft, g_left_zone);
    UpdateDeadZone(kDeadZoneRight, g_right_zone);
    app::GameDeadZone() = g_left_zone.game;
}

// Out of aim the turn waits for the next tick of port 0's camera machine. The follow cameras (on foot, riding, the
// wagon) take the yaw into their bearing and the pitch into the tilt they read; the mounted gun takes both.
// A stick right lowers their angles: the game's yaw curve (0x14DB20) flips the stick's sign.
struct CameraTurn {
    uint32_t machine = 0;
    Turn turn;
    bool ticking = false;
    bool ticked = false;
    bool followed = false;
    bool following = false;
};
CameraTurn g_camera;

uint32_t CameraMachine(uint32_t player) {
    uint32_t camera = R32(player + kPlayerCamera);
    uint32_t rig = camera ? R32(camera + kCameraRig) : 0;
    return rig ? R32(rig + kRigMachine) : 0;
}

uint32_t CameraInput(uint32_t machine, uint32_t index) {
    uint32_t inputs = R32(machine + kMachineInputs);
    return R32(R32(inputs + kInputArray) + 4 * index) + kInputValue;
}

void AddCameraTurn(Turn t) {
    g_camera.turn.x += t.x;
    g_camera.turn.y += t.y;
}

// the player's update fills the inputs twice a frame, so the turn adds up until the tick
void LatchCameraTurn(uint32_t player) {
    g_camera.machine = CameraMachine(player);
    AddCameraTurn(TakeTurn());
}

void ApplyCameraTilt() {
    if (!g_camera.following) return;
    AddFloat(CameraInput(g_camera.machine, kInputCameraTilt), -g_camera.turn.y);
    g_camera.turn.y = 0.0f;
}

float TakeCameraYaw() {
    float yaw = g_camera.turn.x;
    g_camera.turn.x = 0.0f;
    g_camera.followed = true;
    return yaw;
}

}  // namespace

// ioInput update: the pads, then their listeners
X86_EXTERN_FUNC(__imp__sub_00048390);
X86_FUNC(sub_00048390) {
    input::Screen screen = CurrentScreen();
    input::SetScreen(screen);
    uint32_t player = R32(kPlayer);
    input::SetRiding(screen == input::Screen::Play && player && G<uint8_t>(player + kOnHorse));
    g_turn = screen == input::Screen::Play ? TakeMotion() : Turn{};
    if (!g_camera.ticked) g_camera.turn = {};
    g_camera.ticked = false;
    UpdateDeadZones();
    __imp__sub_00048390(ctx, base);
}

// the player's look, thiscall(player, stick) ret 4, before the camera's input; aiming, the stick turns the camera
// too, so the turn goes to both
X86_EXTERN_FUNC(__imp__sub_00143350);
X86_FUNC(sub_00143350) {
    uint32_t player = ctx.ecx;
    uint32_t stick = R32(ctx.esp + 4);
    bool aiming = Aiming(player);
    if (PortZero(player)) {
        input::SetAiming(aiming);
        if (aiming) {
            DropSoftLock(ctx, player);
            Turn t = TakeTurn();
            AddAimTurn(player, stick, t);
            AddCameraTurn(t);
        }
    }
    __imp__sub_00143350(ctx, base);
}

// the duel's aim, thiscall(player): the stick turns the reticle's yaw right and its pitch up, the aim sensitivities
// apply; it runs before the camera's input
X86_EXTERN_FUNC(__imp__sub_00147630);
X86_FUNC(sub_00147630) {
    uint32_t player = ctx.ecx;
    if (PortZero(player)) {
        input::SetAiming(true);
        Turn t = TakeTurn();
        AddFloat(player + kLookYaw, t.x);
        AddFloat(player + kLookPitch, -t.y);
    }
    __imp__sub_00147630(ctx, base);
}

// the duel's stick, thiscall(duel, stick) ret 4: before the reticle the mouse's vertical turn moves the right stick,
// down grabs the gun (back) and up draws it (forward)
X86_EXTERN_FUNC(__imp__sub_00096750);
X86_FUNC(sub_00096750) {
    uint32_t stick = R32(ctx.esp + 4);
    uint32_t player = R32(kPlayer);
    bool drawing = R32(ctx.ecx + kDuelState) < kDuelAiming;
    if (player && stick == player + kDuelStick && drawing && g_turn.y != 0.0f) {
        float flick = std::clamp(g_turn.y / kDrawFlick, -1.0f, 1.0f);
        if (std::abs(flick) > std::abs(ReadFloat(stick + kStickY))) WriteFloat(stick + kStickY, flick);
        g_turn.y = 0.0f;
    }
    __imp__sub_00096750(ctx, base);
}

// the camera machine's inputs from the player, thiscall(player, sticks) ret 4
X86_EXTERN_FUNC(__imp__sub_0013FCF0);
X86_FUNC(sub_0013FCF0) {
    uint32_t player = ctx.ecx;
    __imp__sub_0013FCF0(ctx, base);
    if (PortZero(player)) LatchCameraTurn(player);
}

// the camera machine's tick, thiscall(machine, tunables) ret 4, from the rig's update; a turn its cameras leave is
// dropped, as is one no tick came for since the last pad update
X86_EXTERN_FUNC(__imp__sub_0029C670);
X86_FUNC(sub_0029C670) {
    bool port_zero = ctx.ecx && ctx.ecx == g_camera.machine;
    g_camera.ticking = port_zero;
    g_camera.followed = false;
    if (port_zero) ApplyCameraTilt();
    __imp__sub_0029C670(ctx, base);
    g_camera.ticking = false;
    if (!port_zero) return;
    g_camera.ticked = true;
    g_camera.following = g_camera.followed;
    g_camera.turn = {};
}

// the follow camera's pivot (P_StrafePivot_SetRadius_AngleLimit 0x2A82D0) has just read its bearing, the
// PlatformBearing DOF, onto the x87 stack; it adds the stick's swing next, then its angle limit
void FollowCameraBearing(X86Context& ctx, uint8_t*) {
    if (g_camera.ticking) ctx.st[ctx.top & 7] -= TakeCameraYaw();
}

// the wagon camera's pivot (P_WagonStrafePivot_SetRadius), thiscall(action, context) ret 4
X86_EXTERN_FUNC(__imp__sub_002ACD30);
X86_FUNC(sub_002ACD30) {
    if (g_camera.ticking) AddFloat(ctx.ecx + kWagonSwing, -TakeCameraYaw());
    __imp__sub_002ACD30(ctx, base);
}

// the mounted gun camera (c_gatling), thiscall(action, context) ret 4: it subtracts the stick's turn, then clamps
X86_EXTERN_FUNC(__imp__sub_0029FDA0);
X86_FUNC(sub_0029FDA0) {
    if (g_camera.ticking) {
        Turn t = g_camera.turn;
        g_camera.turn = {};
        AddFloat(ctx.ecx + kGatlingYaw, -t.x);
        AddFloat(ctx.ecx + kGatlingPitch, -t.y);
    }
    __imp__sub_0029FDA0(ctx, base);
}

// the aim's soft lock searches, thiscall(player, ...), from the targeting update (sub_00148D10), a target or 0
#define SOFT_LOCK_SEARCH(name)                                               \
    X86_EXTERN_FUNC(__imp__##name);                                          \
    X86_FUNC(name) {                                                         \
        uint32_t player = ctx.ecx;                                           \
        __imp__##name(ctx, base);                                            \
        if (NoAimAssist(player)) ctx.eax = 0;                                \
    }

SOFT_LOCK_SEARCH(sub_00141D50)
SOFT_LOCK_SEARCH(sub_00142850)
