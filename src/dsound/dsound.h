// the game's DirectSound copy, what the host asks of it
#pragma once

namespace dsound {

// any thread: paused, the mixer stops and so does the APU's sample clock, every voice and stream keeps its place
void SetPaused(bool paused);

}  // namespace dsound
