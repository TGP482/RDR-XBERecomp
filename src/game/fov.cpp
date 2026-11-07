// Field of view in play: the player cameras' angle widens by the setting's tangent over the game's. Cutscenes (the
// cinema shutters, level intros and outros), the attract loop and the theater keep the game's angle. Every view takes
// the output's shape, where the game knows only 4:3 and 16:9. Grass culls by the same widened angle.
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

constexpr float kHalfDegree = 3.14159265f / 360.0f;

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

float FovScale() {
    return std::tan(float(app::Rdr().fov) * kHalfDegree) / std::tan(float(app::kGameFov) * kHalfDegree);
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
    FollowOutputShape();
    uint32_t camera = R32(ctx.ecx + kViewportCamera);
    float scale = FovScale();
    if (!camera || scale == 1.0f || !InPlay()) {
        __imp__sub_ViewportProjection(ctx, base);
        return;
    }
    float& fov = G<float>(camera + kCameraFov);
    float game = fov;
    fov = 2.0f * std::atan(std::tan(game * 0.5f) * scale);
    __imp__sub_ViewportProjection(ctx, base);
    fov = game;
}

// a grass field's footprint in the drawn view, thiscall(field, footprint, view) ret 8: its half width is
// tan(angle) / 2, so the angle is set for that to widen by the same scale
X86_EXTERN_FUNC(__imp__sub_GrassFootprint);
X86_FUNC(sub_GrassFootprint) {
    uint32_t view = R32(kViews + R32(kView) * 4);
    uint32_t camera = view ? R32(view) : 0;
    float scale = FovScale();
    if (!camera || scale == 1.0f || !InPlay()) {
        __imp__sub_GrassFootprint(ctx, base);
        return;
    }
    float& fov = G<float>(R32(camera + kCameraFovSource));
    float game = fov;
    fov = std::atan(std::tan(game) * scale);
    __imp__sub_GrassFootprint(ctx, base);
    fov = game;
}
