// The menus' cursor focuses the option under it as the D-pad would: buttons, combos and sliders, each by its row; in
// the Flash movies (the save popup, the shop) the button nearest it, through the player's own focus.
// Only a move acts, so a page opens on its own selection and the keys keep theirs; a click then presses A.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

#include "app/display.h"
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

// Flash player, while a movie advances: the movie, the buttons the D-pad steps through, their count, the focused
// one's index and object; a display object's character and world x and y; a movie's width and height
constexpr uint32_t kFlashMovie = 0x6855C0;
constexpr uint32_t kFlashButtons = 0x685600;
constexpr uint32_t kFlashButtonCount = 0x6855F8;
constexpr uint32_t kFlashFocusIndex = 0x6855FC;
constexpr uint32_t kFlashFocused = 0x6855DC;
constexpr uint32_t kMaxFlashButtons = 32;
constexpr uint32_t kObjectCharacter = 0x4;
constexpr uint32_t kObjectX = 0x30;
constexpr uint32_t kObjectY = 0x34;
constexpr uint32_t kMovieWidth = 0x1C;
constexpr uint32_t kMovieHeight = 0x24;
// a button's state change, thiscall(character, object, from, to), which runs its rollOver and rollOut
constexpr uint32_t kChangeState = 0xC;
constexpr uint32_t kStateUp = 1;
constexpr uint32_t kStateOver = 2;

void ChangeState(X86Context& ctx, uint32_t object, uint32_t from, uint32_t to) {
    ctx.ecx = R32(object + kObjectCharacter);
    CallGuest(ctx, R32(R32(ctx.ecx) + kChangeState), {object, from, to});
}

// the button nearest the point, in the movie's own units, -1 for none
int NearestButton(float x, float y) {
    uint32_t count = std::min(R32(kFlashButtonCount), kMaxFlashButtons);
    int nearest = -1;
    float best = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t button = R32(kFlashButtons + i * 4);
        float d = std::hypot(G<float>(button + kObjectX) - x, G<float>(button + kObjectY) - y);
        if (nearest < 0 || d < best) {
            nearest = int(i);
            best = d;
        }
    }
    return nearest;
}

void UpdateFlashHover(X86Context& ctx) {
    float x = 0.0f;
    float y = 0.0f;
    uint32_t movie = R32(kFlashMovie);
    if (!movie || !input::MenuCursorAt(x, y) || !UpdateSeen(movie, x, y)) return;
    // the movies draw narrowed to 4:3 in the middle of a wide screen
    float share = app::Wide() ? app::NarrowShare() : 1.0f;
    float movie_x = (0.5f + (x - 0.5f) / share) * G<float>(movie + kMovieWidth);
    int hovered = NearestButton(movie_x, y * G<float>(movie + kMovieHeight));
    int focused = int32_t(R32(kFlashFocusIndex));
    if (hovered < 0 || hovered == focused) return;
    if (focused >= 0 && uint32_t(focused) < R32(kFlashButtonCount)) {
        ChangeState(ctx, R32(kFlashButtons + uint32_t(focused) * 4), kStateOver, kStateUp);
    }
    uint32_t button = R32(kFlashButtons + uint32_t(hovered) * 4);
    ChangeState(ctx, button, kStateUp, kStateOver);
    W32(kFlashFocusIndex, uint32_t(hovered));
    W32(kFlashFocused, button);
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

// the Flash player's D-pad focus, while a movie advances: the buttons it gathers, then the move
X86_EXTERN_FUNC(__imp__sub_001ACDA0);
X86_FUNC(sub_001ACDA0) {
    W32(kFlashButtonCount, 0);  // gathered only while A is up
    __imp__sub_001ACDA0(ctx, base);
    UpdateFlashHover(ctx);
}
