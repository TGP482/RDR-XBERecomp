// The main menu's Xbox Live corner shows the recomp's name in the game's own font, its other texts left blank.
#include <cstdio>
#include <cstring>

#include "app/version.h"
#include "game/watermark.h"
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
constexpr char kWatermark[] = "XBE Recomp";
uint32_t g_watermark = 0;  // its element, the main menu's last

void SetRawText(uint32_t element, const char* text) {
    if (!element) return;
    G<uint8_t>(element + kRawText) = 1;
    std::memcpy(HostPtr(element + kText), text, std::strlen(text) + 1);
}

}  // namespace

// main menu instructions update, thiscall(seconds) ret 4
X86_EXTERN_FUNC(__imp__sub_MainMenuInstructionsUpdate);
X86_FUNC(sub_MainMenuInstructionsUpdate) {
    uint32_t menu = ctx.ecx;
    __imp__sub_MainMenuInstructionsUpdate(ctx, base);
    char watermark[16];
    std::snprintf(watermark, sizeof(watermark), "%s V%d", kWatermark, RDR_VERSION);
    g_watermark = R32(menu + kLiveTitle);
    SetRawText(g_watermark, watermark);
    SetRawText(R32(menu + kLiveButton), "");
    SetRawText(R32(menu + kLiveInfo), "");
    SetRawText(R32(menu + kLiveName), "");
}

// the menu gone, another element may take its place, so its text is checked too
bool game::IsWatermark(uint32_t element) {
    if (!element || element != g_watermark) return false;
    return std::strncmp(static_cast<const char*>(HostPtr(element + kText)), kWatermark, sizeof(kWatermark) - 1) == 0;
}
