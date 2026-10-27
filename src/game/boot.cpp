// Boot: the Rockstar logo video plays before the front end loads.
#include "app/game_settings.h"
#include "xdk/hle.h"

// plays rslogo.bik and waits for its end, fastcall with nothing to return
X86_EXTERN_FUNC(__imp__sub_00017940);
X86_FUNC(sub_00017940) {
    if (!app::Rdr().skip_intros) {
        __imp__sub_00017940(ctx, base);
        return;
    }
    ctx.esp += 4;  // ret
}
