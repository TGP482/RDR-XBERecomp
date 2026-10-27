// XOnline is never started (the SDK's XOnlineStartup does nothing), so its global stays null and XOnlineGetUsers
// fails with XONLINE_E_NOT_INITIALIZED. The game's Live UI start needs it: answer as a console with no stored accounts.
#include <cstring>

#include "xdk/hle.h"

namespace {

constexpr uint32_t kStoredUsersBytes = 16 * 0x70;  // XONLINE_MAX_STORED_ONLINE_USERS of XONLINE_USER

}  // namespace

// (users, count)
HLE_STDCALL(XOnlineGetUsers, 2) {
    std::memset(kernel::HostPtr(a[0]), 0, kStoredUsersBytes);
    kernel::W32(a[1], 0);
    return 0;
}
