// The menus' cursor focuses the option under it as the D-pad would: buttons, combos and sliders, each by its row, and
// in the Flash menus (the save popup's Yes and No, the pre-level Start, Weapon Selection and Shop) through the
// selection their own script steps with Up and Down.
// Only a move acts, so a page opens on its own selection and the keys keep theirs; a click then presses A.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "core/input.h"
#include "game/load_show.h"
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

// the save popup's movie, loaded at +0x28; a movie's player, the player's root object and movie, the movie's height
constexpr uint32_t kPopupMovie = 0x65DB48;
constexpr uint32_t kMovieLoaded = 0x28;
constexpr uint32_t kMoviePlayer = 0x24;
constexpr uint32_t kPlayerRoot = 0x8;
constexpr uint32_t kPlayerMovie = 0x4;
constexpr uint32_t kMovieHeight = 0x24;
// display object: world matrix d and y, first child, next sibling, instance name
constexpr uint32_t kWorldScaleY = 0x2C;
constexpr uint32_t kWorldY = 0x34;
constexpr uint32_t kFirstChild = 0x64;
constexpr uint32_t kNextSibling = 0x68;
constexpr uint32_t kName = 0x6C;
// a movie's _global number as an int, thiscall(movie, name, value out) ret 8, and its set, thiscall(movie, name,
// value) ret 8, both false when there is none
constexpr uint32_t kGetGlobal = 0x189E0;
constexpr uint32_t kSetGlobal = 0x18A00;
// its script's choices, 0 Yes and 1 No: the hand's y in theScreens, selectionVPos + currentSelection * menuLineOffset
constexpr int kPopupChoices = 2;
constexpr int kMaxDepth = 8;  // clips searched below a movie's root

uint32_t GuestString(const char* s) {
    uint32_t va = mem::AllocSystem(uint32_t(std::strlen(s) + 1));
    std::memcpy(HostPtr(va), s, std::strlen(s) + 1);
    return va;
}

bool GetGlobal(X86Context& ctx, uint32_t movie, uint32_t name, int& value) {
    static const uint32_t out = mem::AllocSystem(4);
    ctx.ecx = movie;
    bool found = CallGuest(ctx, kGetGlobal, {name, out}) & 0xFF;
    value = int32_t(R32(out));
    return found;
}

uint32_t ChildNamed(uint32_t parent, const char* name) {
    for (uint32_t child = R32(parent + kFirstChild); child; child = R32(child + kNextSibling)) {
        uint32_t s = R32(child + kName);
        if (s && std::strcmp(static_cast<const char*>(HostPtr(s)), name) == 0) return child;
    }
    return 0;
}

// a clip holding a child of that name, searched from the object down
uint32_t ParentOfChild(uint32_t object, const char* name, int depth = 0) {
    if (ChildNamed(object, name)) return object;
    if (depth == kMaxDepth) return 0;
    for (uint32_t child = R32(object + kFirstChild); child; child = R32(child + kNextSibling)) {
        if (uint32_t found = ParentOfChild(child, name, depth + 1)) return found;
    }
    return 0;
}

// a Flash menu's rows, at first + row * line in the clip's space
struct MovieRows {
    uint32_t clip;
    int first;
    int line;
    int count;
};

// the row under the cursor, nearest when past the ends
int RowUnder(uint32_t movie, const MovieRows& rows, float y) {
    uint32_t def = R32(R32(movie + kMoviePlayer) + kPlayerMovie);
    float scale = G<float>(rows.clip + kWorldScaleY);
    float row0 = float(rows.first) * scale + G<float>(rows.clip + kWorldY);
    float step = float(rows.line) * scale;
    float cursor = y * G<float>(def + kMovieHeight);
    return std::clamp(int(std::lround((cursor - row0) / step)), 0, rows.count - 1);
}

void UpdatePopupHover(X86Context& ctx) {
    static const uint32_t selection = GuestString("currentSelection");
    static const uint32_t top = GuestString("selectionVPos");
    static const uint32_t line = GuestString("menuLineOffset");
    uint32_t movie = R32(kPopupMovie);
    float x = 0.0f;
    float y = 0.0f;
    if (!movie || !G<uint8_t>(movie + kMovieLoaded) || !input::MenuCursorAt(x, y) || !UpdateSeen(movie, x, y)) return;
    uint32_t player = R32(movie + kMoviePlayer);
    uint32_t def = player ? R32(player + kPlayerMovie) : 0;
    uint32_t root = player ? R32(player + kPlayerRoot) : 0;
    uint32_t screens = root ? ChildNamed(root, "theScreens") : 0;
    if (!def || !screens) return;
    int current = 0;
    int first = 0;
    int offset = 0;
    bool found = GetGlobal(ctx, movie, selection, current) && GetGlobal(ctx, movie, top, first);
    if (!found || !GetGlobal(ctx, movie, line, offset) || offset == 0) return;
    int hovered = RowUnder(movie, {screens, first, offset, kPopupChoices}, y);
    if (hovered == current) return;
    ctx.ecx = movie;
    CallGuest(ctx, kSetGlobal, {selection, uint32_t(hovered)});
}

// The pre-level menu's choices, 0 to SelectMax, Shop gone in a bounty: its hand's y in the clip holding the slots,
// selectionStartV + curMenSel * menuLineOffset. Only that movie sets bountyMode.
void UpdateLevelMenuHover(X86Context& ctx, uint32_t movie) {
    static const uint32_t bounty = GuestString("bountyMode");
    static const uint32_t selection = GuestString("curMenSel");
    static const uint32_t top = GuestString("selectionStartV");
    static const uint32_t line = GuestString("menuLineOffset");
    static const uint32_t last = GuestString("SelectMax");
    float x = 0.0f;
    float y = 0.0f;
    int mode = 0;
    if (!game::InLoadShow() || !G<uint8_t>(movie + kMovieLoaded) || !GetGlobal(ctx, movie, bounty, mode)) return;
    if (!input::MenuCursorAt(x, y) || !UpdateSeen(movie, x, y)) return;
    uint32_t player = R32(movie + kMoviePlayer);
    uint32_t def = player ? R32(player + kPlayerMovie) : 0;
    uint32_t root = player ? R32(player + kPlayerRoot) : 0;
    uint32_t slots = root ? ParentOfChild(root, "slot0") : 0;
    if (!def || !slots) return;
    int current = 0;
    int first = 0;
    int offset = 0;
    int max = 0;
    bool found = GetGlobal(ctx, movie, selection, current) && GetGlobal(ctx, movie, top, first);
    found = found && GetGlobal(ctx, movie, line, offset) && GetGlobal(ctx, movie, last, max);
    if (!found || offset == 0 || max < 0) return;
    int hovered = RowUnder(movie, {slots, first, offset, max + 1}, y);
    if (hovered == current) return;
    ctx.ecx = movie;
    CallGuest(ctx, kSetGlobal, {selection, uint32_t(hovered)});
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

// the popups' update, fastcall(popup): boot, load and save, each through its own
X86_EXTERN_FUNC(__imp__sub_000193D0);
X86_FUNC(sub_000193D0) {
    uint32_t popup = ctx.ecx;
    UpdatePopupHover(ctx);
    ctx.ecx = popup;
    __imp__sub_000193D0(ctx, base);
}

// a Flash movie's update, thiscall(movie, seconds) ret 4: its script reads the buttons
X86_EXTERN_FUNC(__imp__sub_00018870);
X86_FUNC(sub_00018870) {
    uint32_t movie = ctx.ecx;
    UpdateLevelMenuHover(ctx, movie);
    ctx.ecx = movie;
    __imp__sub_00018870(ctx, base);
}
