// XOnline is never started (the SDK's XOnlineStartup does nothing), so its global stays null and XOnlineGetUsers
// fails with XONLINE_E_NOT_INITIALIZED. The game's Live UI start needs it: answer as a console with no stored accounts.
#include <cstring>

#include "xdk/hle.h"

namespace {

constexpr uint32_t kStoredUsersBytes = 16 * 0x70;  // XONLINE_MAX_STORED_ONLINE_USERS of XONLINE_USER
// the front end's update reads its pressed buttons here, the front end in esi; its page at +0x708, 1 to 3 the
// book, the poster and the journal
constexpr uint32_t kFrontPressedReturn = 0x40F97;
constexpr uint32_t kFrontPage = 0x708;
constexpr uint32_t kBook = 1;
constexpr uint32_t kJournal = 3;
constexpr uint32_t kButtonX = 0x4000000;
constexpr uint32_t kButtonY = 0x8000000;

}  // namespace

// (users, count)
HLE_STDCALL(XOnlineGetUsers, 2) {
    std::memset(kernel::HostPtr(a[0]), 0, kStoredUsersBytes);
    kernel::W32(a[1], 0);
    return 0;
}

// the page manager's pressed buttons, thiscall(manager): on the book, poster and journal Y ("Options") opens the
// Xbox Live scene, which draws from nothing without Live; it opens the game's options as X does
X86_EXTERN_FUNC(__imp__sub_00028CA0);
X86_FUNC(sub_00028CA0) {
    uint32_t page = kernel::R32(ctx.esp) == kFrontPressedReturn ? kernel::R32(ctx.esi + kFrontPage) : 0;
    __imp__sub_00028CA0(ctx, base);
    if (page >= kBook && page <= kJournal && (ctx.eax & kButtonY)) ctx.eax = (ctx.eax & ~kButtonY) | kButtonX;
}
