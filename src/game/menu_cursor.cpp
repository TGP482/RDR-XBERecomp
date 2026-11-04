// The menus' cursor focuses the option under it as the D-pad would: buttons, combos and sliders, each by its row.
// Only a move acts, so a page opens on its own selection and the keys keep theirs; a click then presses A.
#include <algorithm>
#include <initializer_list>
#include <vector>

#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr float kPageHeight = 448.0f;
constexpr float kMaxRow = 40.0f;  // a row reaches the next one's top, at most this far
// page: active, its widgets, their count, the focused one
constexpr uint32_t kActive = 0x3D;
constexpr uint32_t kWidgets = 0x40;
constexpr uint32_t kWidgetCount = 0x298;
constexpr uint32_t kFocused = 0x31C;
constexpr uint32_t kMaxWidgets = (kWidgetCount - kWidgets) / 4;
// widget: kind, top, focusable when both are set, its focus(quiet) and unfocus slots
constexpr uint32_t kKind = 0x48;
constexpr uint32_t kTop = 0x50;
constexpr uint32_t kSelectable = 0xD8;
constexpr uint32_t kEnabled = 0xE6;
constexpr uint32_t kFocus = 0x10;
constexpr uint32_t kUnfocus = 0x14;
// a slider has no place of its own: its linked widgets, its title first
constexpr uint32_t kSlider = 0xF;
constexpr uint32_t kSliderLinks = 0x114;
constexpr uint32_t kSliderLinkCount = 0x118;

struct Row {
    int index;
    float top;
};

// where each page last saw the cursor; a page not seen yet starts from it
struct Seen {
    uint32_t page;
    float x;
    float y;
};
std::vector<Seen> g_seen;
constexpr size_t kMaxSeen = 8;

// true when the cursor moved since the page last saw it
bool UpdateSeen(uint32_t page, float x, float y) {
    auto it = std::find_if(g_seen.begin(), g_seen.end(), [&](const Seen& s) { return s.page == page; });
    if (it == g_seen.end()) {
        if (g_seen.size() == kMaxSeen) g_seen.erase(g_seen.begin());
        g_seen.push_back({page, x, y});
        return false;
    }
    bool moved = it->x != x || it->y != y;
    it->x = x;
    it->y = y;
    return moved;
}

float RowTop(uint32_t widget) {
    uint32_t links = R32(widget + kSliderLinks);
    bool slider = R32(widget + kKind) == kSlider && links && G<uint16_t>(widget + kSliderLinkCount);
    return G<float>((slider ? R32(links) : widget) + kTop);
}

std::vector<Row> Rows(uint32_t page) {
    std::vector<Row> rows;
    uint32_t count = std::min(R32(page + kWidgetCount), kMaxWidgets);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t widget = R32(page + kWidgets + i * 4);
        if (!widget || !G<uint8_t>(widget + kSelectable) || !G<uint8_t>(widget + kEnabled)) continue;
        rows.push_back({int(i), RowTop(widget)});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.top < b.top; });
    return rows;
}

// the widget whose row the point is in, -1 for none
int Under(uint32_t page, float y) {
    std::vector<Row> rows = Rows(page);
    for (size_t r = 0; r < rows.size(); r++) {
        float bottom = rows[r].top + kMaxRow;
        if (r + 1 < rows.size()) bottom = std::min(bottom, rows[r + 1].top);
        if (y >= rows[r].top && y < bottom) return rows[r].index;
    }
    return -1;
}

void CallSlot(X86Context& ctx, uint32_t widget, uint32_t slot, std::initializer_list<uint32_t> args) {
    ctx.ecx = widget;
    CallGuest(ctx, R32(R32(widget) + slot), args);
}

void UpdateHover(X86Context& ctx, uint32_t page) {
    float x = 0.0f;
    float y = 0.0f;
    if (!input::MenuCursorAt(x, y) || !UpdateSeen(page, x, y)) return;
    int hovered = Under(page, y * kPageHeight);
    int focused = int(R32(page + kFocused));
    if (hovered < 0 || hovered == focused) return;
    if (focused >= 0 && uint32_t(focused) < R32(page + kWidgetCount)) {
        CallSlot(ctx, R32(page + kWidgets + uint32_t(focused) * 4), kUnfocus, {});
    }
    CallSlot(ctx, R32(page + kWidgets + uint32_t(hovered) * 4), kFocus, {0});
    W32(page + kFocused, uint32_t(hovered));
}

}  // namespace

// a page's update, thiscall(page, ?) ret 4, before its input
X86_EXTERN_FUNC(__imp__sub_0001AE90);
X86_FUNC(sub_0001AE90) {
    uint32_t page = ctx.ecx;
    if (G<uint8_t>(page + kActive)) UpdateHover(ctx, page);
    ctx.ecx = page;
    __imp__sub_0001AE90(ctx, base);
}
