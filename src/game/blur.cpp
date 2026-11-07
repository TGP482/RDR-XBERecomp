// The screen blur the game draws over play: its pass is skipped as the No Distortion cheat skips it. Its pieces cover
// the screen as the game places them, never narrowed as 4:3 2D.
#include "app/game_settings.h"
#include "d3d/inline_shift.h"
#include "xdk/hle.h"

// the blur pass, a vtable slot (0x3D61CC) jumping to 0x1156D0, thiscall with nothing to return
X86_EXTERN_FUNC(__imp__sub_BlurPass);
X86_FUNC(sub_BlurPass) {
    if (!app::Rdr().screen_blur) {
        ctx.esp += 4;  // ret
        return;
    }
    xdk::BeginInlineKeep();
    __imp__sub_BlurPass(ctx, base);
    xdk::EndInlineKeep();
}
