// The main menu's Xbox Live corner shows the recomp's name in the game's own font, its other texts left blank.
#include <cstdio>
#include <cstring>

#include "app/version.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// main menu instructions: its Xbox Live text, button, sign in and gamertag elements
constexpr uint32_t kLiveTitle = 0x454;
constexpr uint32_t kLiveButton = 0x458;
constexpr uint32_t kLiveInfo = 0x45C;
constexpr uint32_t kLiveName = 0x460;
// text element: 1 draws the text as written, 0 looks it up in the string table; the text
constexpr uint32_t kRawText = 0x120;
constexpr uint32_t kText = 0x121;

void SetRawText(uint32_t element, const char* text) {
    if (!element) return;
    *static_cast<uint8_t*>(HostPtr(element + kRawText)) = 1;
    std::memcpy(HostPtr(element + kText), text, std::strlen(text) + 1);
}

}  // namespace

// main menu instructions update, thiscall(seconds) ret 4
X86_EXTERN_FUNC(__imp__sub_00039E00);
X86_FUNC(sub_00039E00) {
    uint32_t menu = ctx.ecx;
    __imp__sub_00039E00(ctx, base);
    char watermark[16];
    std::snprintf(watermark, sizeof(watermark), "XBE Recomp V%d", RDR_VERSION);
    SetRawText(R32(menu + kLiveTitle), watermark);
    SetRawText(R32(menu + kLiveButton), "");
    SetRawText(R32(menu + kLiveInfo), "");
    SetRawText(R32(menu + kLiveName), "");
}
