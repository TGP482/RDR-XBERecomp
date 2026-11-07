// Widescreen HUD: the game's 4:3 2D keeps its shape in the middle of the screen (d3d/d3d8.cpp), and each HUD element
// goes to the side of the screen it sat on last frame, so the HUD keeps to the screen's edges. In the front end only the
// button prompts, the warning popups, the difficulty select, the profile page, the Flash movies (title, shop, upgrades)
// and the pages on a 4:3 picture (options) narrow, in the middle; the legal screen's and those pages' pictures narrow
// over black anywhere. Full screen movies follow FMV SCALE.
#include <bit>
#include <cstring>

#include "app/app.h"
#include "app/display.h"
#include "app/game_settings.h"
#include "core/input.h"
#include "d3d/inline_shift.h"
#include "game/boot.h"
#include "game/load_show.h"
#include "game/prompts.h"
#include "game/swui.h"
#include "game/watermark.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace swui;

namespace {

constexpr hle::Va kFrontendButtonsVtable = SYM(FrontendButtonsVtable);  // swuiPageFrontendButtons, the button prompts
constexpr hle::Va kWarningFrontendVtable = SYM(WarningFrontendVtable);  // the front end's warning popups (new game over a save)
constexpr hle::Va kDifficultyVtable = SYM(DifficultyVtable);       // difficultyselect, the new game's NORMAL / HARD box
// the IPU background image's vtable and texture
constexpr hle::Va kImageVtable = SYM(ImageVtable);
constexpr uint32_t kImageTexture = 0x114;
constexpr float kSide = 1.0f / 3;  // clip space: an element centred past this goes to its side
constexpr float kFullWidth = 0.98f;

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
constexpr hle::Va kMovieDrawReturn = SYM(MovieDrawReturn);
constexpr hle::Va kLogoLoopReturn = SYM(LogoLoopReturn);
// stage 0's texture, cdecl(texture); a textured rect, cdecl(x0, y0, x1, y1, u0, v0, u1, v1, colour, 0, 0, 0) in
// pixels; the screen's size
constexpr hle::Va kSetTexture = SYM(SetFirstStageTexture);
constexpr hle::Va kDrawRect = SYM(DrawRect);
constexpr hle::Va kScreenWidth = SYM(ScreenWidth);
constexpr hle::Va kScreenHeight = SYM(ScreenHeight);
// a movie's frame is NTSC 4:3 whatever its pixels
constexpr float kMovieAspect = 4.0f / 3.0f;

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
constexpr hle::Va kMovieName = SYM(MovieName);  // the playing movie's name

uint32_t BarRows(uint32_t height) {
    const char* name = static_cast<const char*>(HostPtr(kMovieName));
    uint32_t rows = kCutsceneBars;
    for (const MovieBars& m : kMovieBars) {
        if (std::strstr(name, m.name)) rows = m.rows;
    }
    return rows * height / 480;
}

// The movie's 4:3 frame zoomed as far as its black bars allow: a picture wider than the screen fills its width,
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
    float screen_shape = app::OutputAspect();
    float shown = 1.0f;  // share of the screen's width
    if (picture >= screen_shape) crop = uint32_t(float(height) * (1 - kMovieAspect / screen_shape) / 2);
    else shown = picture / screen_shape;
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

HUD_ELEMENT(sub_HudDontShoot)
HUD_ELEMENT(sub_HudMoney)
HUD_ELEMENT(sub_HudNames)
HUD_ELEMENT(sub_HudPauseScroll)
HUD_ELEMENT(sub_HudGoals)
HUD_ELEMENT(sub_HudHitArrows)
HUD_ELEMENT(sub_HudHealth)
HUD_ELEMENT(sub_HudAmmo)
HUD_ELEMENT(sub_HudCrosshair)

// the bridge's cannon counts, thiscall(hud, ...): each team's icon and count goes to its own side
static Element g_cannons[2];
static Element* g_cannon = nullptr;

X86_EXTERN_FUNC(__imp__sub_HudCannon);
X86_FUNC(sub_HudCannon) {
    g_cannon = &g_cannons[0];
    BeginElement(*g_cannon);
    __imp__sub_HudCannon(ctx, base);
    EndElement(*g_cannon);
}

// the second team's count is about to draw
void CannonCountSecondTeam(X86Context&, uint8_t*) {
    EndElement(*g_cannon);
    g_cannon = &g_cannons[1];
    BeginElement(*g_cannon);
}

// a health bar and its name, cdecl(enemy, place, ...), drawn by sub_001D9550: the enemies' (bosses') and the allies'
// each go to their own side
X86_EXTERN_FUNC(__imp__sub_HudHealthBar);
X86_FUNC(sub_HudHealthBar) {
    static Element bars[2];
    Element& bar = bars[R32(ctx.esp + 4) ? 1 : 0];
    BeginElement(bar);
    __imp__sub_HudHealthBar(ctx, base);
    EndElement(bar);
}

// the HUD's 2D width scale (game/fov.cpp), and the viewports' shape a kept draw scales by as the console does
constexpr hle::Va kHudShape = SYM(HudShape);
constexpr hle::Va kViewportShape = SYM(ViewportShape);

// the letterbox, thiscall(hud): its bars and torn edges already span the screen, kept
X86_EXTERN_FUNC(__imp__sub_HudLetterbox);
X86_FUNC(sub_HudLetterbox) {
    float hud_shape = G<float>(kHudShape);
    G<float>(kHudShape) = G<float>(kViewportShape);
    xdk::BeginInlineKeep();
    __imp__sub_HudLetterbox(ctx, base);
    xdk::EndInlineKeep();
    G<float>(kHudShape) = hud_shape;
}

// the lock reticle, thiscall(reticle): centred on its target in the 3D view, kept
X86_EXTERN_FUNC(__imp__sub_LockReticle);
X86_FUNC(sub_LockReticle) {
    xdk::BeginInlineKeep();
    __imp__sub_LockReticle(ctx, base);
    xdk::EndInlineKeep();
}

// the journal's draw over the 3D book, thiscall(page): its widgets and Flash already fit the book, kept
X86_EXTERN_FUNC(__imp__sub_JournalDraw);
X86_FUNC(sub_JournalDraw) {
    xdk::BeginInlineKeep();
    __imp__sub_JournalDraw(ctx, base);
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
X86_EXTERN_FUNC(__imp__sub_PageWidgets);
X86_FUNC(sub_PageWidgets) {
    uint32_t vtable = R32(ctx.ecx);
    bool buttons = vtable == kFrontendButtonsVtable;
    bool popup = vtable == kWarningFrontendVtable || vtable == kDifficultyVtable;
    bool narrow = buttons || popup || OnPicture(ctx.ecx);
    if (buttons) game::BeginPromptBar(ctx.ecx);
    if (narrow) BeginMiddle();
    __imp__sub_PageWidgets(ctx, base);
    if (narrow) EndMiddle();
    if (buttons) game::EndPromptBar();
}

// a text element's draw, fastcall(element): the main menu's watermark goes to its side as a HUD element
X86_EXTERN_FUNC(__imp__sub_TextElementDraw);
X86_FUNC(sub_TextElementDraw) {
    static Element watermark;
    if (!game::IsWatermark(ctx.ecx)) {
        __imp__sub_TextElementDraw(ctx, base);
        return;
    }
    BeginElement(watermark);
    __imp__sub_TextElementDraw(ctx, base);
    EndElement(watermark);
}

// the console's 16:9 flag (game/fov.cpp)
constexpr hle::Va kWide = SYM(Wide);

// The profile page's draw, thiscall(page): its widgets, then the name grid's letters, narrowed. Its 16:9 path (40
// pixels right, a 2D scale of 1) is the console's own for its 640 wide screen, left out.
X86_EXTERN_FUNC(__imp__sub_ProfilePageDraw);
X86_FUNC(sub_ProfilePageDraw) {
    uint8_t wide = G<uint8_t>(kWide);
    G<uint8_t>(kWide) = 0;
    BeginMiddle();
    __imp__sub_ProfilePageDraw(ctx, base);
    EndMiddle();
    G<uint8_t>(kWide) = wide;
}

// a page's IPU background, fastcall(image): a whole screen 4:3 picture (options_bg_10k), narrowed over black
X86_EXTERN_FUNC(__imp__sub_PageBackground);
X86_FUNC(sub_PageBackground) {
    if (!app::Wide() || !R32(ctx.ecx + kImageTexture)) {
        __imp__sub_PageBackground(ctx, base);
        return;
    }
    uint32_t image = ctx.ecx;
    DrawBlackScreen(ctx, 0, 0);
    xdk::EndInlineSide();
    xdk::BeginInlineSide(0, xdk::Fit::Whole);
    ctx.ecx = image;
    __imp__sub_PageBackground(ctx, base);
    xdk::EndInlineSide();
    if (g_depth > 0) xdk::BeginInlineSide(0);
}

// a Flash movie's draw, thiscall(movie); the load show's (the level's results, the shop) are whole 4:3 pictures on
// the frame it clears black
X86_EXTERN_FUNC(__imp__sub_FlashMovieDraw);
X86_FUNC(sub_FlashMovieDraw) {
    if (game::TitleHidden()) {
        ctx.esp += 4;  // ret
        return;
    }
    if (g_depth == 0 && game::InLoadShow()) {
        xdk::BeginInlineSide(0, xdk::Fit::Whole);
        __imp__sub_FlashMovieDraw(ctx, base);
        xdk::EndInlineSide();
        return;
    }
    BeginMiddle();
    __imp__sub_FlashMovieDraw(ctx, base);
    EndMiddle();
}

// a load show frame, cdecl(looping): from the front end its state is still FrontEnd, which narrows only the hooked
// draws, so its 2D narrows as after a level
X86_EXTERN_FUNC(__imp__sub_LoadShowFrame);
X86_FUNC(sub_LoadShowFrame) {
    xdk::BeginInlineLevel();
    __imp__sub_LoadShowFrame(ctx, base);
    xdk::EndInlineLevel();
}

// the load show's sun is about to take the console's 16:9 x squash (0.75): the 2D narrows to 4:3 already
bool LoadShowSunRound(X86Context&, uint8_t*) { return true; }

// the screen's clear, cdecl(flags, colour, z, stencil)
constexpr hle::Va kClear = SYM(Clear);
constexpr uint32_t kClearTarget = 0xF0;

// the legal screen's picture (rdrlogoshot), 4:3, drawn over the whole screen; before a level the front end's last
// frame is still there, so the sides clear black first
X86_EXTERN_FUNC(__imp__sub_LogoPicture);
X86_FUNC(sub_LogoPicture) {
    if (game::TitleHidden()) {
        ctx.esp += 4;  // ret
        return;
    }
    if (g_depth > 0) {
        __imp__sub_LogoPicture(ctx, base);
        return;
    }
    if (app::Wide()) CallGuest(ctx, kClear, {kClearTarget, 0xFF000000u, 0, 0});
    xdk::BeginInlineSide(0, xdk::Fit::Whole);
    __imp__sub_LogoPicture(ctx, base);
    xdk::EndInlineSide();
}

// A full screen movie's picture, thiscall(movie, x, y) ret 8, drawn by sub_00070F60. The game fills the 4:3 screen
// with the movie's 4:3 frame, so a widescreen display stretches it. FMV SCALE's Fill zooms the frame as far as its
// black bars allow instead; 4:3 narrows the game's picture to 4:3 as the legal screen's. The Rockstar logo stays 4:3.
X86_EXTERN_FUNC(__imp__sub_MoviePicture);
X86_FUNC(sub_MoviePicture) {
    bool logo = R32(ctx.esp) == kMovieDrawReturn && R32(ctx.esp + 0xC) == kLogoLoopReturn;
    if (!app::Wide()) {
        __imp__sub_MoviePicture(ctx, base);
        return;
    }
    if (logo || app::Rdr().fmv_scale == 1) {
        DrawBlackScreen(ctx, R32(ctx.esp + 4), R32(ctx.esp + 8));
        xdk::BeginInlineSide(0, xdk::Fit::Whole);
        __imp__sub_MoviePicture(ctx, base);
        xdk::EndInlineSide();
        return;
    }
    DrawMovieFilled(ctx);
    ctx.esp += 12;  // ret 8
}

// the tutorial controller's camera, thiscall(camera, left, right, bottom, top, near, far): its making sets x from
// minus the wide shape to a fixed 16:9, so it is set again across the output's own shape
constexpr uint32_t kControllerCamera = 0x4C;
constexpr hle::Va kSetOrtho = SYM(SetOrtho);
constexpr uint32_t kMinusOne = 0xBF800000;
constexpr uint32_t kOne = 0x3F800000;
constexpr uint32_t kMinusTen = 0xC1200000;
constexpr uint32_t kTen = 0x41200000;

// the tutorial controller's making, thiscall(controller)
X86_EXTERN_FUNC(__imp__sub_TutorialControllerMake);
X86_FUNC(sub_TutorialControllerMake) {
    uint32_t controller = ctx.ecx;
    __imp__sub_TutorialControllerMake(ctx, base);
    if (!app::Wide()) return;
    uint32_t made = ctx.eax;
    float shape = app::OutputAspect();
    uint32_t left = std::bit_cast<uint32_t>(-shape);
    uint32_t right = std::bit_cast<uint32_t>(shape);
    ctx.ecx = R32(controller + kControllerCamera);
    CallGuest(ctx, kSetOrtho, {left, right, kMinusOne, kOne, kMinusTen, kTen});
    ctx.eax = made;
}

// the tutorial controller's place, an index into the tuned positions (x, y, z) it moves to (sub_001E0030)
constexpr uint32_t kControllerPlace = 0x50;
constexpr hle::Va kControllerPositions = SYM(ControllerPositions);
constexpr uint32_t kPositionBytes = 0xC;

// the side of the place it moves to, as on 4:3, so its whole move in keeps to that side
static int ControllerSide(uint32_t controller) {
    uint32_t place = R32(controller + kControllerPlace);
    float x = std::bit_cast<float>(R32(kControllerPositions + place * kPositionBytes));
    float middle = x / app::kConsoleAspect;
    if (middle < -kSide) return -1;
    if (middle > kSide) return 1;
    return 0;
}

// the tutorial's controller, thiscall(controller), vtable 0x3F5850 slot 6: the game keeps its shape on 16:9 but lays
// it out from the middle, so it moves against the side of where it is going; not drawn on the keyboard and mouse
X86_EXTERN_FUNC(__imp__sub_TutorialControllerDraw);
X86_FUNC(sub_TutorialControllerDraw) {
    if (input::KeyboardActive()) {
        ctx.esp += 4;  // ret
        return;
    }
    if (g_depth > 0) {
        __imp__sub_TutorialControllerDraw(ctx, base);
        return;
    }
    xdk::BeginInlineSide(ControllerSide(ctx.ecx), xdk::Fit::Shift);
    __imp__sub_TutorialControllerDraw(ctx, base);
    xdk::EndInlineSide();
}
