// The frame rate follows MAX FRAME RATE alone: the game's own waits for the console's frame are gone.
#include "xdk/hle.h"

// the frame's end (sub_0017D3A0) has swapped and would wait for its vertical blanks, one in play, two in cutscenes
bool SkipVBlankWait(X86Context&, uint8_t*) { return true; }
