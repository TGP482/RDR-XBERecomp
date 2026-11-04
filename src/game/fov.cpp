// Field of view in play: the player cameras' angle widens by the setting's tangent over the game's. Cutscenes (the
// cinema shutters, level intros and outros), the attract loop and the theater keep the game's angle. Every view takes
// the output's shape, where the game knows only 4:3 and 16:9.
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
// set while the shutters close for a cutscene (sub_00052450)
constexpr uint32_t kCinema = 0x65E7F0;

constexpr float kHalfDegree = 3.14159265f / 360.0f;

// set from the console's video flags (sub_0017E370): widescreen, the 16:9 shape it gives the views, and the
// viewports' shape over the screen's pixels
constexpr uint32_t kWide = 0x683568;
constexpr uint32_t kWideShape = 0x3D4DB4;
constexpr uint32_t kViewportShape = 0x413D18;
constexpr uint32_t kScreenWidth = 0x413C44;
constexpr uint32_t kScreenHeight = 0x413C48;

void FollowOutputShape() {
    float shape = app::OutputAspect();
    G<uint8_t>(kWide) = app::Wide();
    G<float>(kWideShape) = shape;
    uint32_t width = R32(kScreenWidth);
    if (width) G<float>(kViewportShape) = float(R32(kScreenHeight)) / float(width) * shape;
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
X86_EXTERN_FUNC(__imp__sub_00052A70);
X86_FUNC(sub_00052A70) {
    FollowOutputShape();
    uint32_t camera = R32(ctx.ecx + kViewportCamera);
    float scale = FovScale();
    if (!camera || scale == 1.0f || !InPlay()) {
        __imp__sub_00052A70(ctx, base);
        return;
    }
    float& fov = G<float>(camera + kCameraFov);
    float game = fov;
    fov = 2.0f * std::atan(std::tan(game * 0.5f) * scale);
    __imp__sub_00052A70(ctx, base);
    fov = game;
}
