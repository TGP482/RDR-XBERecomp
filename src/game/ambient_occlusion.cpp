// Ambient occlusion goes in before each view's first alpha blended bucket: every opaque surface is lit and in the
// depth buffer, and translucent surfaces and the HUD land on top.
#include "app/game_settings.h"
#include "d3d/occlusion.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// render buckets: their array, 0x654 each; the first drawn alpha blended (sub_00071650 sets 11 in a level); the view
// being drawn, -1 with a single view; the presented frames' count
constexpr uint32_t kBuckets = 0x65E7B0;
constexpr uint32_t kBucketSize = 0x654;
constexpr uint32_t kFirstAlphaBucket = 0x65E7A8;
constexpr uint32_t kView = 0x65E778;
constexpr uint32_t kFrameCount = 0x683ABC;
constexpr uint32_t kViews = 4;

uint32_t g_done[kViews] = {};  // the frame count each view last had it at, plus one

}  // namespace

// a bucket's draw, thiscall(bucket, order)
X86_EXTERN_FUNC(__imp__sub_00051830);
X86_FUNC(sub_00051830) {
    uint32_t mode = app::Rdr().ambient_occlusion;
    uint32_t view = R32(kView);
    if (view >= kViews) view = 0;
    int32_t bucket = int32_t(ctx.ecx - R32(kBuckets)) / int32_t(kBucketSize);
    uint32_t frame = R32(kFrameCount) + 1;
    if (mode && bucket >= int32_t(R32(kFirstAlphaBucket)) && g_done[view] != frame) {
        g_done[view] = frame;
        xdk::Occlusion(mode == 2);
    }
    __imp__sub_00051830(ctx, base);
}
