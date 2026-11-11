// Exit on the main menu, under Player Options or under Bounty Hunter while that shows; its button is read after
// Bounty Hunter's line (game/options_menu.cpp). It asks with the front end's own warning popup, the one New Game
// over a save uses, its question swapped for ours. Yes closes the window.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "app/app.h"
#include "app/translations.h"
#include "game/exit_menu.h"
#include "game/swui.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace swui;

namespace {

// the story page's buttons Exit goes under
constexpr uint32_t kButton = 5;
constexpr char kOptionsButton[] = "story_playeropt";
constexpr char kBountyButton[] = "story_bhunter";
// story page: the front end; front end: its warning popup up until it closes, and the answer, 1 Yes (sub_000383A0)
constexpr uint32_t kStoryFrontEnd = 0x44C;
constexpr uint32_t kFrontEndWarning = 0x120;
constexpr uint32_t kFrontEndAnswer = 0x124;
// the warning popup's question, a text widget: drawn as written instead of looked up, and its text or string name
constexpr hle::Va kWarningFrontendVtable = SYM(WarningFrontendVtable);
constexpr char kNewGameQuestion[] = "story_confirm_new";
constexpr uint32_t kText = 3;
constexpr uint32_t kTextLiteral = 0x120;
constexpr uint32_t kTextString = 0x121;
constexpr size_t kTextMax = 0x82;
constexpr char kQuitQuestion[] = "Are you sure you want to quit?";

bool g_asking = false;

uint32_t WidgetAt(uint32_t page, int index) { return R32(page + kPageWidgets + uint32_t(index) * 4); }

int WidgetIndex(uint32_t page, uint32_t kind, const char* name) {
    for (uint32_t i = 0; i < R32(page + kPageWidgetCount); i++) {
        uint32_t widget = WidgetAt(page, int(i));
        if (!widget || R32(widget + kWidgetKind) != kind) continue;
        if (std::strcmp(static_cast<const char*>(HostPtr(widget + kWidgetName)), name) == 0) return int(i);
    }
    return -1;
}

// the game's language in latin 1, as the game draws text it has no string for
std::string InGameLanguage(const char* english) {
    const char16_t* t = app::Translation(english);
    if (!t) return english;
    std::string out;
    for (; *t; t++) out += *t < 0x100 ? char(*t) : '?';
    return out;
}

// Bounty Hunter's row while it is hidden, else the row under it
void UpdateExitRow(uint32_t quit, uint32_t bounty, uint32_t options) {
    float bounty_top = G<float>(bounty + kTop);
    float row = bounty_top - G<float>(options + kTop);
    G<float>(quit + kTop) = G<uint8_t>(bounty + kWidgetShown) ? bounty_top + row : bounty_top;
}

}  // namespace

void game::UpdateExitButton(uint32_t page) {
    int options = WidgetIndex(page, kButton, kOptionsButton);
    int bounty = WidgetIndex(page, kButton, kBountyButton);
    uint32_t front = R32(page + kStoryFrontEnd);
    if (options < 0 || bounty < 0 || !front || uint32_t(bounty + 1) >= R32(page + kPageWidgetCount)) return;
    uint32_t quit = WidgetAt(page, bounty + 1);
    if (!quit) return;
    UpdateExitRow(quit, WidgetAt(page, bounty), WidgetAt(page, options));
    if (g_asking) {
        if (G<uint8_t>(front + kFrontEndWarning)) return;
        g_asking = false;
        if (R32(front + kFrontEndAnswer) == 1) PostMessageW(static_cast<HWND>(app::Window()), WM_CLOSE, 0, 0);
        return;
    }
    bool focused = R32(page + kPageFocused) == uint32_t(bounty + 1);
    if (!focused || !(R32(page + kPagePressed) & kButtonA)) return;
    W32(front + kFrontEndAnswer, 0);
    G<uint8_t>(front + kFrontEndWarning) = 1;
    g_asking = true;
}

void game::UpdateExitQuestion(uint32_t page) {
    if (R32(page) != kWarningFrontendVtable) return;
    int index = WidgetIndex(page, kText, kNewGameQuestion);
    if (index < 0) return;
    uint32_t text = WidgetAt(page, index);
    std::string shown = g_asking ? InGameLanguage(kQuitQuestion) : kNewGameQuestion;
    shown.resize(std::min(shown.size(), kTextMax - 1));
    std::memcpy(HostPtr(text + kTextString), shown.c_str(), shown.size() + 1);
    G<uint8_t>(text + kTextLiteral) = g_asking;
}
