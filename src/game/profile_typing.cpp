// The profile page's name takes keys as they are typed: letters go in, Space puts a space, Backspace deletes and Enter
// ends the name, as the grid's letters, its < and > and End do. Arrows and a click still work the grid.
#include <algorithm>
#include <cctype>
#include <string>

#include "core/input.h"
#include "game/swui.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace swui;

namespace {

// profile page: typing a name, the grid's row and column
constexpr uint32_t kNaming = 0x44E;
constexpr uint32_t kGridRow = 0x46C;
constexpr uint32_t kGridColumn = 0x470;
// the grid's columns; End's place in "ABCDEFGHIJKLMNOPQRSTUVWXYZ<>" then End
constexpr hle::Va kGridColumns = SYM(GridColumns);
constexpr uint32_t kGridEnd = 28;
// the grid's letter into the name, thiscall(page, letter) ret 4: '<' deletes, '>' a space
constexpr hle::Va kEnterLetter = SYM(EnterLetter);

void EnterLetter(X86Context& ctx, uint32_t page, char letter) {
    ctx.ecx = page;
    CallGuest(ctx, kEnterLetter, {uint32_t(uint8_t(letter))});
}

// the grid's cursor to End, then A ends the name as the game does
void PressEnd(uint32_t page) {
    uint32_t columns = std::max(R32(kGridColumns), 1u);
    W32(page + kGridRow, kGridEnd / columns);
    W32(page + kGridColumn, kGridEnd % columns);
    W32(page + kPagePressed, R32(page + kPagePressed) | kButtonA);
}

void TypeName(X86Context& ctx, uint32_t page) {
    input::KeepTextEntry();
    for (char c : input::TakeTyped()) {
        if (std::isalpha(uint8_t(c))) EnterLetter(ctx, page, char(std::toupper(uint8_t(c))));
        else if (c == ' ') EnterLetter(ctx, page, '>');
        else if (c == '\b') EnterLetter(ctx, page, '<');
        else if (c == '\r') PressEnd(page);
    }
}

}  // namespace

// the profile page's input, thiscall(page), its pressed buttons read
X86_EXTERN_FUNC(__imp__sub_ProfilePageInput);
X86_FUNC(sub_ProfilePageInput) {
    uint32_t page = ctx.ecx;
    if (G<uint8_t>(page + kNaming)) TypeName(ctx, page);
    ctx.ecx = page;
    __imp__sub_ProfilePageInput(ctx, base);
}
