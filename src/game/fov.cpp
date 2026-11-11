// Field of view in play: the player cameras' angle widens by the setting's tangent over the game's. Cutscenes (the
// cinema shutters, level intros and outros), the attract loop and the theater keep the game's angle. Every view takes
// the output's shape, where the game knows only 4:3 and 16:9. Grass culls by the drawn view's width. LETTERBOX off
// takes the game's own unbordered view, the same width shown taller.
#include <cmath>

#include "app/display.h"
#include "app/game_settings.h"
#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// viewport: its camera; camera: its angle in radians, across a 4:3 frame
constexpr uint32_t kViewportCamera = 0x30;
constexpr uint32_t kCameraFov = 0x38;
// the views (sub_001F3640), +0 each one's camera, and the view being drawn; camera: where its angle is read from,
// its own by default (sub_001F4F20)
constexpr hle::Va kViews = SYM(Views);
constexpr hle::Va kView = SYM(View);
constexpr uint32_t kCameraFovSource = 0x68;
// set while the shutters close for a cutscene (sub_00052450)
constexpr hle::Va kCinema = SYM(Cinema);
// set as a level loads: the view draws between borders (sub_00052620); clear, it fills the screen
constexpr hle::Va kLetterbox = SYM(Letterbox);
bool g_letterbox_held = false;  // the game's flag, taken away while LETTERBOX is off

constexpr float kHalfDegree = 3.14159265f / 360.0f;

// out of cutscenes the view's height stretches over the 400 lines between 40 line borders (sub_000529C0)
constexpr float kBorderStretch = 480.0f / 400.0f;
constexpr float kBorderShape = 1.0f + (kBorderStretch - 1.0f) / 480.0f;

// set from the console's video flags (sub_0017E370): widescreen, the 16:9 shape it gives the views, and the
// viewports' shape over the screen's pixels
constexpr hle::Va kWide = SYM(Wide);
constexpr hle::Va kWideShape = SYM(WideShape);
constexpr hle::Va kViewportShape = SYM(ViewportShape);
constexpr hle::Va kScreenWidth = SYM(ScreenWidth);
constexpr hle::Va kScreenHeight = SYM(ScreenHeight);
// the HUD's copy of the viewports' shape (sub_001DCA70), the scale of its 2D widths; its 2D shows at 4:3, so it lays
// out the console's 640 wide screen across the screen's pixels as its positions do
constexpr hle::Va kHudShape = SYM(HudShape);
constexpr float kConsoleWidth = 640.0f;

void FollowOutputShape() {
    float shape = app::OutputAspect();
    G<uint8_t>(kWide) = app::Wide();
    G<float>(kWideShape) = shape;
    uint32_t width = R32(kScreenWidth);
    if (!width) return;
    G<float>(kViewportShape) = float(R32(kScreenHeight)) / float(width) * shape;
    G<float>(kHudShape) = float(width) / kConsoleWidth;
}

void UpdateLetterbox() {
    uint8_t& flag = G<uint8_t>(kLetterbox);
    if (!app::Rdr().letterbox) {
        g_letterbox_held = g_letterbox_held || flag;
        flag = 0;
        return;
    }
    if (g_letterbox_held) flag = 1;
    g_letterbox_held = false;
}

float FovScale() {
    return std::tan(float(app::Rdr().fov) * kHalfDegree) / std::tan(float(app::kGameFov) * kHalfDegree);
}

// LETTERBOX off: the unbordered projection (0x52C85) would show another width than the bordered one (0x52655, or
// 0x52685 in a widescreen cutscene), so the angle is set for the bordered width. Borders are 40 of the 480 lines, 20
// in a widescreen cutscene (sub_000529C0).
constexpr float kConsoleLines = 480.0f;
constexpr float kBorderLines = 40.0f;
constexpr float kCinemaBorderLines = 20.0f;
constexpr float kBorderedGrowth = 0.5f;
constexpr float kUnborderedGrowth = 0.63f;
constexpr float kCinemaShape = 8.0f / 9.0f;

float Unbordered(float fov) {
    bool cinema = G<uint8_t>(kCinema) && G<uint8_t>(kWide);
    float lines = cinema ? kCinemaBorderLines : kBorderLines;
    float stretch = kConsoleLines / (kConsoleLines - 2.0f * lines);
    float shape = cinema ? G<float>(kWideShape) : app::kConsoleAspect;
    float half = std::tan(fov * 0.5f);
    float bordered_shape = ((stretch - 1.0f) * kBorderedGrowth + 1.0f) * shape;
    float width = cinema ? half / (kCinemaShape * shape) : half * stretch / bordered_shape;
    float unbordered_shape = ((stretch - 1.0f) * kUnborderedGrowth + 1.0f) * shape;
    return 2.0f * std::atan(unbordered_shape * std::tan(std::atan(width) / stretch));
}

float Widened(float fov) {
    return 2.0f * std::atan(std::tan(fov * 0.5f) * FovScale());
}

// the view's half width at unit depth as the projection builds it out of cutscenes: the camera's angle sets the
// height across a 4:3 frame, stretched by the borders, then the shape sets the width
float DrawnHalfWidth(float fov, float shape) {
    float halfHeight = std::atan(std::tan(fov * 0.5f) / (kBorderShape * app::kConsoleAspect)) * kBorderStretch;
    return std::tan(halfHeight) * shape;
}

bool InPlay() {
    input::Screen screen = input::GetScreen();
    bool playing = screen == input::Screen::Play || screen == input::Screen::Pause;
    return playing && !G<uint8_t>(kCinema);
}

}  // namespace

// a viewport's projection from its camera, thiscall(viewport, clear) ret 4
X86_EXTERN_FUNC(__imp__sub_ViewportProjection);
X86_FUNC(sub_ViewportProjection) {
    UpdateLetterbox();
    FollowOutputShape();
    uint32_t camera = R32(ctx.ecx + kViewportCamera);
    bool widen = InPlay() && FovScale() != 1.0f;
    if (!camera || (!widen && !g_letterbox_held)) {
        __imp__sub_ViewportProjection(ctx, base);
        return;
    }
    float& fov = G<float>(camera + kCameraFov);
    float game = fov;
    float drawn = widen ? Widened(game) : game;
    fov = g_letterbox_held ? Unbordered(drawn) : drawn;
    __imp__sub_ViewportProjection(ctx, base);
    fov = game;
}

// a grass field's footprint in the drawn view, thiscall(field, footprint, view) ret 8: its half width is
// tan(angle) / 2, so the angle is set for that to widen as the drawn view does over the console's 4:3 one
X86_EXTERN_FUNC(__imp__sub_GrassFootprint);
X86_FUNC(sub_GrassFootprint) {
    uint32_t view = R32(kViews + R32(kView) * 4);
    uint32_t camera = view ? R32(view) : 0;
    if (!camera || G<uint8_t>(kCinema)) {
        __imp__sub_GrassFootprint(ctx, base);
        return;
    }
    float& fov = G<float>(R32(camera + kCameraFovSource));
    float game = fov;
    float drawn = InPlay() ? Widened(game) : game;
    float scale = DrawnHalfWidth(drawn, app::OutputAspect()) / DrawnHalfWidth(game, app::kConsoleAspect);
    fov = std::atan(std::tan(game) * scale);
    __imp__sub_GrassFootprint(ctx, base);
    fov = game;
}
