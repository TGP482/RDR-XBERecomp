// Widescreen HUD: the game's 4:3 2D keeps its shape in the middle of the screen (d3d/d3d8.cpp), and each HUD element
// goes to the side of the screen it sat on last frame, so the HUD keeps to the screen's edges. In the front end only the
// button prompts, the Flash movies (title, shop, upgrades) and the pages on a 4:3 picture (options) narrow, in the
// middle; the legal screen's and those pages' pictures narrow over black anywhere. Full screen movies follow FMV SCALE.
#include <cstring>

#include "app/app.h"
#include "app/game_settings.h"
#include "d3d/inline_shift.h"
#include "game/boot.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kFrontendButtonsVtable = 0x3D3500;  // swuiPageFrontendButtons, the button prompts
// a swui page's widgets and their count; the IPU background image's vtable and texture
constexpr uint32_t kPageWidgets = 0x40;
constexpr uint32_t kPageWidgetCount = 0x298;
constexpr uint32_t kImageVtable = 0x3D0E18;
constexpr uint32_t kImageTexture = 0x114;
constexpr float kSide = 1.0f / 3;  // clip space: an element centred past this goes to its side
constexpr float kFullWidth = 0.98f;
constexpr float kNarrow = 0.75f;  // 4:3 over 16:9

struct Element {
    int side = 0;
};

int g_depth = 0;

int SideOf(const xdk::ScreenSpan& span) {
    if (span.x0 <= -kFullWidth && span.x1 >= kFullWidth) return 0;
    float middle = (span.x0 + span.x1) / 2;
    if (middle < -kSide) return -1;
    if (middle > kSide) return 1;
    return 0;
}

void BeginElement(const Element& e) {
    if (g_depth++ == 0) xdk::BeginInlineSide(e.side);
}

void EndElement(Element& e) {
    if (--g_depth > 0) return;
    xdk::ScreenSpan span = xdk::EndInlineSide();
    if (span.drawn) e.side = SideOf(span);
}

// movie: +0 its Bink (+0 width, +4 height), +4 its texture; the movie draw's return, in sub_00070F60, and that
// draw's return in the Rockstar logo's loop (sub_00017940)
constexpr uint32_t kMovieBink = 0x0;
constexpr uint32_t kMovieTexture = 0x4;
constexpr uint32_t kMovieDrawReturn = 0x70F74;
constexpr uint32_t kLogoLoopReturn = 0x179FE;
// stage 0's texture, cdecl(texture); a textured rect, cdecl(x0, y0, x1, y1, u0, v0, u1, v1, colour, 0, 0, 0) in
// pixels; the screen's size
constexpr uint32_t kSetTexture = 0x182AB0;
constexpr uint32_t kDrawRect = 0x17D8B0;
constexpr uint32_t kScreenWidth = 0x413C44;
constexpr uint32_t kScreenHeight = 0x413C48;
// a movie's frame is NTSC 4:3 whatever its pixels
constexpr float kMovieAspect = 4.0f / 3.0f;
constexpr float kWideAspect = 16.0f / 9.0f;

// the screen black, so a 4:3 picture leaves no old frame at its sides; full width draws keep their place
void DrawBlackScreen(X86Context& ctx, uint32_t x, uint32_t y) {
    CallGuest(ctx, kSetTexture, {0});
    CallGuest(ctx, kDrawRect, {x, y, x + R32(kScreenWidth), y + R32(kScreenHeight), 0, 0, 0, 0, 0xFF000000u, 0, 0, 0});
}

// black rows above and below a movie's picture, of its 480, measured: the cutscenes' unless listed
struct MovieBars {
    const char* name;
    uint32_t rows;
};
constexpr MovieBars kMovieBars[] = {{"titles", 66}, {"attract", 91}, {"slide01", 0}};
constexpr uint32_t kCutsceneBars = 20;
constexpr uint32_t kMovieName = 0x666558;  // the playing movie's name

uint32_t BarRows(uint32_t height) {
    const char* name = static_cast<const char*>(HostPtr(kMovieName));
    uint32_t rows = kCutsceneBars;
    for (const MovieBars& m : kMovieBars) {
        if (std::strstr(name, m.name)) rows = m.rows;
    }
    return rows * height / 480;
}

// The movie's 4:3 frame zoomed as far as its black bars allow: a picture wider than 16:9 fills the screen's width,
// its rows cropped evenly; a narrower one fills its height between black pillars. Kept as placed.
void DrawMovieFilled(X86Context& ctx) {
    uint32_t movie = ctx.ecx;
    uint32_t texture = R32(movie + kMovieTexture);
    uint32_t bink = R32(movie + kMovieBink);
    if (!texture || !bink) return;
    uint32_t width = R32(bink);
    uint32_t height = R32(bink + 4);
    uint32_t crop = BarRows(height);
    float picture = kMovieAspect * float(height) / float(height - 2 * crop);
    float shown = 1.0f;  // share of the screen's width
    if (picture >= kWideAspect) crop = uint32_t(float(height) * (1 - kMovieAspect / kWideAspect) / 2);
    else shown = picture / kWideAspect;
    float screen = float(R32(kScreenWidth));
    uint32_t x = R32(ctx.esp + 4);
    uint32_t y = R32(ctx.esp + 8);
    uint32_t x0 = x + uint32_t(screen * (1 - shown) / 2);
    uint32_t x1 = x + uint32_t(screen * (1 + shown) / 2);
    xdk::BeginInlineKeep();
    if (shown < 1.0f) DrawBlackScreen(ctx, x, y);
    CallGuest(ctx, kSetTexture, {texture});
    CallGuest(ctx, kDrawRect, {x0, y, x1, y + R32(kScreenHeight), 0, crop, width, height - crop, 0xFFFFFFFFu, 0, 0, 0});
    CallGuest(ctx, kSetTexture, {0});
    xdk::EndInlineKeep();
}

void BeginMiddle() {
    if (g_depth++ == 0) xdk::BeginInlineSide(0);
}

void EndMiddle() {
    if (--g_depth == 0) xdk::EndInlineSide();
}

}  // namespace

// the elements the HUD's draw (sub_001DAB00) calls
#define HUD_ELEMENT(name)              \
    X86_EXTERN_FUNC(__imp__##name);    \
    X86_FUNC(name) {                   \
        static Element element;        \
        BeginElement(element);         \
        __imp__##name(ctx, base);      \
        EndElement(element);           \
    }

HUD_ELEMENT(sub_001D5000)
HUD_ELEMENT(sub_001D60E0)
HUD_ELEMENT(sub_001D75A0)
HUD_ELEMENT(sub_001D8D60)
HUD_ELEMENT(sub_001D9550)
HUD_ELEMENT(sub_001D96D0)
HUD_ELEMENT(sub_001DE080)
HUD_ELEMENT(sub_001DEF00)
HUD_ELEMENT(sub_001E42D0)
HUD_ELEMENT(sub_001E54D0)
HUD_ELEMENT(sub_001E7FB0)

// the letterbox, thiscall(hud): its bars and torn edges already span the screen, kept
X86_EXTERN_FUNC(__imp__sub_001D7120);
X86_FUNC(sub_001D7120) {
    xdk::BeginInlineKeep();
    __imp__sub_001D7120(ctx, base);
    xdk::EndInlineKeep();
}

// the lock reticle, thiscall(reticle): centred on its target in the 3D view, kept
X86_EXTERN_FUNC(__imp__sub_001E2330);
X86_FUNC(sub_001E2330) {
    xdk::BeginInlineKeep();
    __imp__sub_001E2330(ctx, base);
    xdk::EndInlineKeep();
}

// the journal's draw over the 3D book, thiscall(page): its widgets and Flash already fit the book, kept
X86_EXTERN_FUNC(__imp__sub_00037970);
X86_FUNC(sub_00037970) {
    xdk::BeginInlineKeep();
    __imp__sub_00037970(ctx, base);
    xdk::EndInlineKeep();
}

// a page on an IPU background, a 4:3 picture: the options pages; other pages lie over the front end's 3D desk
static bool OnPicture(uint32_t page) {
    for (uint32_t i = 0; i < R32(page + kPageWidgetCount); i++) {
        uint32_t widget = R32(page + kPageWidgets + i * 4);
        if (widget && R32(widget) == kImageVtable) return true;
    }
    return false;
}

// a swui page's widgets, thiscall(page)
X86_EXTERN_FUNC(__imp__sub_00019CC0);
X86_FUNC(sub_00019CC0) {
    bool narrow = R32(ctx.ecx) == kFrontendButtonsVtable || OnPicture(ctx.ecx);
    if (narrow) BeginMiddle();
    __imp__sub_00019CC0(ctx, base);
    if (narrow) EndMiddle();
}

// a page's IPU background, fastcall(image): a whole screen 4:3 picture (options_bg_10k), narrowed over black
X86_EXTERN_FUNC(__imp__sub_000217D0);
X86_FUNC(sub_000217D0) {
    if (!app::Widescreen() || !R32(ctx.ecx + kImageTexture)) {
        __imp__sub_000217D0(ctx, base);
        return;
    }
    uint32_t image = ctx.ecx;
    DrawBlackScreen(ctx, 0, 0);
    xdk::EndInlineSide();
    xdk::BeginInlineSide(0, xdk::Fit::Whole);
    ctx.ecx = image;
    __imp__sub_000217D0(ctx, base);
    xdk::EndInlineSide();
    if (g_depth > 0) xdk::BeginInlineSide(0);
}

// a Flash movie's draw, thiscall(movie)
X86_EXTERN_FUNC(__imp__sub_000188A0);
X86_FUNC(sub_000188A0) {
    if (game::TitleHidden()) {
        ctx.esp += 4;  // ret
        return;
    }
    BeginMiddle();
    __imp__sub_000188A0(ctx, base);
    EndMiddle();
}

// the legal screen's picture (rdrlogoshot), 4:3, drawn over the whole screen
X86_EXTERN_FUNC(__imp__sub_0005EB40);
X86_FUNC(sub_0005EB40) {
    if (game::TitleHidden()) {
        ctx.esp += 4;  // ret
        return;
    }
    if (g_depth > 0) {
        __imp__sub_0005EB40(ctx, base);
        return;
    }
    xdk::BeginInlineSide(0, xdk::Fit::Whole);
    __imp__sub_0005EB40(ctx, base);
    xdk::EndInlineSide();
}

// A full screen movie's picture, thiscall(movie, x, y) ret 8, drawn by sub_00070F60. The game fills the 4:3 screen
// with the movie's 4:3 frame, so a widescreen display stretches it. FMV SCALE's Fill zooms the frame as far as its
// black bars allow instead; 4:3 narrows the game's picture to 4:3 as the legal screen's. The Rockstar logo stays 4:3.
X86_EXTERN_FUNC(__imp__sub_001F86D0);
X86_FUNC(sub_001F86D0) {
    bool logo = R32(ctx.esp) == kMovieDrawReturn && R32(ctx.esp + 0xC) == kLogoLoopReturn;
    if (!app::Widescreen()) {
        __imp__sub_001F86D0(ctx, base);
        return;
    }
    if (logo || app::Rdr().fmv_scale == 1) {
        DrawBlackScreen(ctx, R32(ctx.esp + 4), R32(ctx.esp + 8));
        xdk::BeginInlineSide(0, xdk::Fit::Whole);
        __imp__sub_001F86D0(ctx, base);
        xdk::EndInlineSide();
        return;
    }
    DrawMovieFilled(ctx);
    ctx.esp += 12;  // ret 8
}

// the tutorial's controller, thiscall(controller), vtable 0x3F5850 slot 6: the game keeps its shape on 16:9 but lays
// it out from the middle, so it moves against its side; the side comes from where it would sit on 4:3
X86_EXTERN_FUNC(__imp__sub_001E03C0);
X86_FUNC(sub_001E03C0) {
    static Element controller;
    if (g_depth > 0) {
        __imp__sub_001E03C0(ctx, base);
        return;
    }
    xdk::BeginInlineSide(controller.side, xdk::Fit::Shift);
    __imp__sub_001E03C0(ctx, base);
    xdk::ScreenSpan span = xdk::EndInlineSide();
    span.x0 /= kNarrow;
    span.x1 /= kNarrow;
    if (span.drawn) controller.side = SideOf(span);
}
