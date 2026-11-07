// The game's SWUI menu pages and their widgets.
#pragma once
#include <initializer_list>

#include "xdk/hle.h"

namespace swui {

// page: its widgets, their count, the pad's pressed buttons, the focused widget
constexpr uint32_t kPageWidgets = 0x40;
constexpr uint32_t kPageWidgetCount = 0x298;
constexpr uint32_t kPagePressed = 0x308;
constexpr uint32_t kPageFocused = 0x31C;
// widget: its kind; its focus(quiet) and unfocus slots
constexpr uint32_t kWidgetKind = 0x48;
constexpr uint32_t kFocus = 0x10;
constexpr uint32_t kUnfocus = 0x14;
// pressed buttons
constexpr uint32_t kButtonUp = 0x1000;
constexpr uint32_t kButtonDown = 0x2000;
constexpr uint32_t kButtonLeft = 0x4000;
constexpr uint32_t kButtonRight = 0x8000;
constexpr uint32_t kButtonA = 0x1000000;
constexpr uint32_t kButtonX = 0x4000000;
constexpr uint32_t kButtonY = 0x8000000;

inline void CallSlot(X86Context& ctx, uint32_t widget, uint32_t slot, std::initializer_list<uint32_t> args) {
    ctx.ecx = widget;
    kernel::CallGuest(ctx, kernel::R32(kernel::R32(widget) + slot), args);
}

}  // namespace swui
