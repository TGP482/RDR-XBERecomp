// Boot: the Rockstar logo video, then the titles video before the press start screen.
#include "app/game_settings.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kTitlesMovie = 0x3D03EC;  // "titles"

}  // namespace

// plays rslogo.bik and waits for its end, fastcall with nothing to return
X86_EXTERN_FUNC(__imp__sub_00017940);
X86_FUNC(sub_00017940) {
    if (!app::Rdr().skip_intros) {
        __imp__sub_00017940(ctx, base);
        return;
    }
    ctx.esp += 4;  // ret
}

// opens a movie, cdecl(names, count, ...), 1 once playing; at 0 the boot's titles loop ends on its first frame
X86_EXTERN_FUNC(__imp__sub_00071080);
X86_FUNC(sub_00071080) {
    uint32_t name = R32(R32(ctx.esp + 4));
    if (!app::Rdr().skip_intros || name != kTitlesMovie) {
        __imp__sub_00071080(ctx, base);
        return;
    }
    ctx.eax = 0;
    ctx.esp += 4;  // ret
}
