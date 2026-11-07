// Debug level pick: a level clicked on the panel's DEBUG page loads at once from the front end or a level, as the
// book's story start does: the session for one player, the level, then straight into the load state.
#include <cstdint>
#include <cstring>

#include "app/menu.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// a LevelFlow entry
constexpr uint32_t kLevelName = 0x0;
constexpr uint32_t kLevelSublevel = 0x4;
// the state machine: its current state, the states a pick loads from and the load state (NewLoadShow)
constexpr uint32_t kCurrentState = 0x6664AC;
constexpr uint32_t kFrontEndState = 0x6664B0;
constexpr uint32_t kGameState = 0x666344;
constexpr uint32_t kLoadState = 0x666374;
// a state's vtable: Enter() returns a transition to take at once or 0, Exit(next) ret 4
constexpr uint32_t kStateEnter = 0x4;
constexpr uint32_t kStateExit = 0x8;
constexpr uint32_t kTransitionTarget = 0x8;
// NewLoadShow's single player start, thiscall(level) ret 4, the level being the name buffer SetLevel fills
constexpr uint32_t kLoadSinglePlayer = 0x65250;
constexpr uint32_t kCurrentLevel = 0x65EB10;
// the front end's session: thiscall reset, thiscall commit; its first player as the story start fills it
constexpr uint32_t kSession = 0x65DDD8;
constexpr uint32_t kResetSession = 0x40100;
constexpr uint32_t kCommitSession = 0x40170;
constexpr uint32_t kSessionHuman = 0x0;  // 0 commits the player as computer controlled
constexpr uint32_t kSessionCharacter = 0x1;  // the level's character code
constexpr uint32_t kSessionProfile = 0x10;
constexpr uint32_t kSessionActive = 0x14;
constexpr uint32_t kSessionPlayers = 0x554;  // players made, committed to 0x65EAF4
constexpr uint32_t kSessionMapPlayers = 0x558;  // picks the m2 or m4 maps, committed to 0x65EAC0
constexpr uint32_t kCurrentProfile = 0x65F02C;
// cdecl(name, sublevel): the level's character code; SetLevel
constexpr uint32_t kLevelCharacter = 0x57340;
constexpr uint32_t kSetLevel = 0x56AF0;
// 0 single player; 1 when a story start sets the level
constexpr uint32_t kGameMode = 0x65EAD0;
constexpr uint32_t kStoryStart = 0x65EAF0;

void SetByte(uint32_t addr, uint8_t value) { *static_cast<uint8_t*>(HostPtr(addr)) = value; }

void StartSession(X86Context& ctx, uint32_t name, uint32_t sublevel) {
    ctx.ecx = kSession;
    CallGuest(ctx, kResetSession, {});
    W32(kSession + kSessionProfile, R32(kCurrentProfile));
    W32(kSession + kSessionPlayers, 1);
    W32(kSession + kSessionMapPlayers, 1);
    SetByte(kSession + kSessionActive, 1);
    uint32_t character = CallGuest(ctx, kLevelCharacter, {name, sublevel});
    char* slot = static_cast<char*>(HostPtr(kSession + kSessionCharacter));
    std::strcpy(slot, static_cast<const char*>(HostPtr(character)));
    SetByte(kSession + kSessionHuman, 1);
    W32(kGameMode, 0);
    SetByte(kStoryStart, 1);
    CallGuest(ctx, kSetLevel, {name, sublevel});
    ctx.ecx = kSession;
    CallGuest(ctx, kCommitSession, {});
}

// the state machine's own switch: the old state's Exit, then Enter down any transitions it takes at once
uint32_t SwitchState(X86Context& ctx, uint32_t from, uint32_t to) {
    ctx.ecx = from;
    CallGuest(ctx, R32(R32(from) + kStateExit), {to});
    for (;;) {
        ctx.ecx = to;
        uint32_t transition = CallGuest(ctx, R32(R32(to) + kStateEnter), {});
        if (!transition) return to;
        to = R32(transition + kTransitionTarget);
    }
}

}  // namespace

// one step of the game's state machine, no arguments: on the front end or in a level a pick loads instead
X86_EXTERN_FUNC(__imp__sub_StateMachineStep);
X86_FUNC(sub_StateMachineStep) {
    uint32_t state = R32(kCurrentState);
    bool loadable = state == R32(kFrontEndState) || state == R32(kGameState);
    uint32_t entry = loadable ? app::TakeLevelPick() : 0;
    if (!entry) {
        __imp__sub_StateMachineStep(ctx, base);
        return;
    }
    StartSession(ctx, R32(entry + kLevelName), R32(entry + kLevelSublevel));
    uint32_t load = R32(kLoadState);
    ctx.ecx = load;
    CallGuest(ctx, kLoadSinglePlayer, {kCurrentLevel});
    W32(kCurrentState, SwitchState(ctx, state, load));
    ctx.esp += 4;  // ret
}
