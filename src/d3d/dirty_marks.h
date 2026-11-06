// Bumped with every gpu::MarkDirty, so a dirty epoch read since then is still current.
#pragma once
#include <atomic>
#include <cstdint>

namespace gpu {

inline std::atomic<uint64_t> g_dirty_marks{0};

}  // namespace gpu
