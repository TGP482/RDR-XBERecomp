// The frame rate follows MAX FRAME RATE alone: the game's own waits for the console's frame are gone. The steps it
// takes each frame as a console frame's length take the frame's real length instead, never more, so they keep the
// console's speed at any frame rate. What counts frames runs once for each console frame gone by.
#include <algorithm>
#include <bit>
#include <chrono>

#include "xdk/hle.h"

using namespace kernel;

namespace {

// the frame counter (sub_00084100 adds one a frame); the last frame's length in seconds
constexpr uint32_t kFrameCount = 0x666738;
constexpr uint32_t kFrameSeconds = 0x4109CC;
// vertical blanks a console frame waits for, 0 with the novblank switch; a vertical blank's length in seconds
constexpr uint32_t kBlanksPerFrame = 0x413C58;
constexpr uint32_t kBlankSeconds = 0x41424C;
// the most console frames one frame catches up
constexpr double kMaxConsoleFrames = 6;

double g_frame_seconds = 1.0;  // until a frame is measured the console's step wins

void MeasureFrame() {
    using clock = std::chrono::steady_clock;
    static clock::time_point last = clock::now();
    clock::time_point now = clock::now();
    g_frame_seconds = std::chrono::duration<double>(now - last).count();
    last = now;
}

double FrameStep(double console) { return std::min(console, g_frame_seconds); }

uint32_t g_console_frame = 0;
uint32_t g_new_console_frames = 0;
double g_console_frame_part = 0;

float ConsoleFrameSeconds() { return float(std::max(R32(kBlanksPerFrame), 1u)) * GF(kBlankSeconds); }

void CountConsoleFrames() {
    double frames = g_console_frame_part + g_frame_seconds / ConsoleFrameSeconds();
    g_console_frame_part = std::min(frames, kMaxConsoleFrames);
    g_new_console_frames = uint32_t(g_console_frame_part);
    g_console_frame_part -= g_new_console_frames;
    g_console_frame += g_new_console_frames;
}

}  // namespace

// the frame's end (sub_0017D3A0) has swapped and would wait for its vertical blanks, one in play, two in cutscenes
bool SkipVBlankWait(X86Context&, uint8_t*) {
    MeasureFrame();
    CountConsoleFrames();
    return true;
}

// the press start screen's frame has drawn and would sleep out the rest of a console frame (sub_00068E90)
bool SkipPressStartSleep(X86Context&, uint8_t*) { return true; }

// the screen transition (sub_001069A0) and the fade after a load (sub_00070590) have loaded their console step
void FrameStepSt0(X86Context& ctx, uint8_t*) { X87_ST(0) = FrameStep(X87_ST(0)); }

// the load show (sub_00067590) has read its console step into eax
void FrameStepEax(X86Context& ctx, uint8_t*) {
    ctx.eax = std::bit_cast<uint32_t>(float(FrameStep(std::bit_cast<float>(ctx.eax))));
}

// the scene's random light flicker, thiscall(scene): it fires on frame counts, so it runs once a console frame, with
// the console frame's length
X86_EXTERN_FUNC(__imp__sub_00107A30);
X86_FUNC(sub_00107A30) {
    uint32_t scene = ctx.ecx;
    uint32_t ret = R32(ctx.esp);
    uint32_t frames = R32(kFrameCount);
    uint32_t seconds = R32(kFrameSeconds);
    uint32_t console = std::bit_cast<uint32_t>(ConsoleFrameSeconds());
    uint32_t first = g_console_frame - g_new_console_frames + 1;
    if (!g_new_console_frames) ctx.esp += 4;  // ret
    for (uint32_t i = 0; i < g_new_console_frames; i++) {
        if (i) {
            ctx.esp -= 4;
            W32(ctx.esp, ret);
        }
        ctx.ecx = scene;
        W32(kFrameCount, first + i);
        W32(kFrameSeconds, console);
        __imp__sub_00107A30(ctx, base);
    }
    W32(kFrameCount, frames);
    W32(kFrameSeconds, seconds);
}
