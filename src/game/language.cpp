// The game's language comes from the console's (XC_LANGUAGE), which XGetLanguage reads at boot: the LANGUAGE setting's.
#include "app/game_settings.h"
#include "xdk/hle.h"

namespace {

// XC_LANGUAGE by setting: English 1, French 4, German 3, Italian 6, Spanish 5
constexpr uint32_t kXcLanguage[] = {1, 1, 4, 3, 6, 5};

}  // namespace

// XGetLanguage, cdecl with no arguments; a release with one language keeps the console's
X86_EXTERN_FUNC(__imp__sub_XGetLanguage);
X86_FUNC(sub_XGetLanguage) {
    if (!HAS(LanguageSetting)) {
        __imp__sub_XGetLanguage(ctx, base);
        return;
    }
    ctx.eax = kXcLanguage[app::BootLanguage()];
    ctx.esp += 4;  // ret
}
