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
// pause page: its Xbox Live button; a widget's active slot, thiscall(widget, active) ret 4
constexpr uint32_t kPauseLiveButton = 0x458;
constexpr uint32_t kSetActive = 0x4C;

}  // namespace

// (users, count)
HLE_STDCALL(XOnlineGetUsers, 2) {
    std::memset(kernel::HostPtr(a[0]), 0, kStoredUsersBytes);
    kernel::W32(a[1], 0);
    return 0;
}

// the page manager's pressed buttons, thiscall(manager): on the book, poster and journal Y ("Options") opens the
// Xbox Live scene, which draws from nothing without Live; it opens the game's options as X does
X86_EXTERN_FUNC(__imp__sub_PageManagerButtons);
X86_FUNC(sub_PageManagerButtons) {
    uint32_t page = kernel::R32(ctx.esp) == kFrontPressedReturn ? kernel::R32(ctx.esi + kFrontPage) : 0;
    __imp__sub_PageManagerButtons(ctx, base);
    if (page >= kBook && page <= kJournal && (ctx.eax & kButtonY)) ctx.eax = (ctx.eax & ~kButtonY) | kButtonX;
}

// the pause page finds its buttons, thiscall(page): Xbox Live goes inactive, as the level map does without a map, so
// the rows close up over it
X86_EXTERN_FUNC(__imp__sub_PausePageButtons);
X86_FUNC(sub_PausePageButtons) {
    uint32_t page = ctx.ecx;
    __imp__sub_PausePageButtons(ctx, base);
    uint32_t live = kernel::R32(page + kPauseLiveButton);
    if (!live) return;
    uint32_t eax = ctx.eax;
    ctx.ecx = live;
    kernel::CallGuest(ctx, kernel::R32(kernel::R32(live) + kSetActive), {0});
    ctx.eax = eax;
}
