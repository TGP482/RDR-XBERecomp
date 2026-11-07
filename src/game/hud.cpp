// The toggle HUD key turns on the game's own No HUD cheat while it hides the HUD, single player only as the cheat is.
// The panel's debug invincibility turns on its Invulnerable cheat the same way.
#include "app/game_settings.h"
#include "app/menu.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kNoHudCheat = 14;
constexpr uint32_t kInvulnerableCheat = 6;

bool Forced(uint32_t bit) {
    if (bit == kNoHudCheat) return app::HudHidden();
    return bit == kInvulnerableCheat && app::Invincible();
}

}  // namespace

// a cheat's bit in the profile, cdecl(player, bit)
X86_EXTERN_FUNC(__imp__sub_CheatBit);
X86_FUNC(sub_CheatBit) {
    uint32_t bit = R32(ctx.esp + 8);
    if (!Forced(bit)) {
        __imp__sub_CheatBit(ctx, base);
        return;
    }
    ctx.eax = 1;
    ctx.esp += 4;  // ret
}
