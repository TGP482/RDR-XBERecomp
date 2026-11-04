// The frame rate follows MAX FRAME RATE alone: the game's own waits for the console's frame are gone. The steps it
// takes each frame as a console frame's length take the frame's real length instead, never more, so they keep the
// console's speed at any frame rate.
#include <algorithm>
#include <bit>
#include <chrono>

#include "xdk/hle.h"

namespace {

double g_frame_seconds = 1.0;  // until a frame is measured the console's step wins

void MeasureFrame() {
    using clock = std::chrono::steady_clock;
    static clock::time_point last = clock::now();
    clock::time_point now = clock::now();
    g_frame_seconds = std::chrono::duration<double>(now - last).count();
    last = now;
}

double FrameStep(double console) { return std::min(console, g_frame_seconds); }

}  // namespace

// the frame's end (sub_0017D3A0) has swapped and would wait for its vertical blanks, one in play, two in cutscenes
bool SkipVBlankWait(X86Context&, uint8_t*) {
    MeasureFrame();
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
