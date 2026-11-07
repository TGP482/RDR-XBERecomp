// Boot: the Rockstar logo video, the titles video, then the title movie's legal and press start screens.
#include "game/boot.h"

#include "app/game_settings.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr hle::Va kTitlesMovie = SYM(TitlesMovie);  // "titles"
constexpr hle::Va kPlayerChoice = SYM(PlayerChoice);  // "playerChoice", logo01X sets it to 1 once start is pressed

bool g_title_hidden = false;

}  // namespace

bool game::TitleHidden() { return g_title_hidden; }

// plays rslogo.bik and waits for its end, fastcall with nothing to return
X86_EXTERN_FUNC(__imp__sub_PlayRockstarLogo);
X86_FUNC(sub_PlayRockstarLogo) {
    if (!app::Rdr().skip_intros) {
        __imp__sub_PlayRockstarLogo(ctx, base);
        return;
    }
    ctx.esp += 4;  // ret
}

// opens a movie, cdecl(names, count, ...), 1 once playing; at 0 the boot's titles loop ends on its first frame
X86_EXTERN_FUNC(__imp__sub_OpenMovie);
X86_FUNC(sub_OpenMovie) {
    uint32_t name = R32(R32(ctx.esp + 4));
    if (!app::Rdr().skip_intros || name != kTitlesMovie) {
        __imp__sub_OpenMovie(ctx, base);
        return;
    }
    ctx.eax = 0;
    ctx.esp += 4;  // ret
}

// a Flash movie variable, thiscall(movie, name, out) ret 8, true when set: the boot waits on playerChoice
X86_EXTERN_FUNC(__imp__sub_FlashGetVariable);
X86_FUNC(sub_FlashGetVariable) {
    if (!app::Rdr().skip_intros || R32(ctx.esp + 4) != kPlayerChoice) {
        __imp__sub_FlashGetVariable(ctx, base);
        return;
    }
    W32(R32(ctx.esp + 8), 1);
    ctx.eax = 1;
    ctx.esp += 12;  // ret 8
}

// the titles video, legal and press start screens, fastcall with nothing to return
X86_EXTERN_FUNC(__imp__sub_TitleScreens);
X86_FUNC(sub_TitleScreens) {
    g_title_hidden = app::Rdr().skip_intros;
    __imp__sub_TitleScreens(ctx, base);
    g_title_hidden = false;
}
