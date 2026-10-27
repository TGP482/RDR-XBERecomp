// The toggle HUD key turns on the game's own No HUD cheat while it hides the HUD, single player only as the cheat is.
#include "app/game_settings.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kNoHudCheat = 14;

}  // namespace

// a cheat's bit in the profile, cdecl(player, bit)
X86_EXTERN_FUNC(__imp__sub_00057070);
X86_FUNC(sub_00057070) {
    uint32_t bit = R32(ctx.esp + 8);
    if (bit != kNoHudCheat || !app::HudHidden()) {
        __imp__sub_00057070(ctx, base);
        return;
    }
    ctx.eax = 1;
    ctx.esp += 4;  // ret
}
