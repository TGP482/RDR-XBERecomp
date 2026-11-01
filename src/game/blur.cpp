// The screen blur the game draws over play: its pass is skipped as the No Distortion cheat skips it.
#include "app/game_settings.h"
#include "xdk/hle.h"

// the blur pass, a vtable slot (0x3D61CC) jumping to 0x1156D0, thiscall with nothing to return
X86_EXTERN_FUNC(__imp__sub_0005F0B0);
X86_FUNC(sub_0005F0B0) {
    if (app::Rdr().screen_blur) {
        __imp__sub_0005F0B0(ctx, base);
        return;
    }
    ctx.esp += 4;  // ret
}
