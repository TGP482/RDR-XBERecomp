// The menus' cursor focuses the option under it as the D-pad would: buttons, combos and sliders, each by its row, and
// in the Flash menus (the save popup's Yes and No, the pre-level Start, Weapon Selection and Shop, the level's end
// Continue, Replay and Quit) through the selection their own script steps with Up and Down. The front end's book
// pages lie on a 3D book: the row comes from where the renderer saw the cursor on the book's page texture.
// Only a move acts, so a page opens on its own selection and the keys keep theirs; a click then presses A.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "core/input.h"
#include "d3d/pick.h"
#include "game/exit_menu.h"
#include "game/load_show.h"
#include "game/start_prompt.h"
#include "game/swui.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace swui;

namespace {

constexpr float kPageWidth = 640.0f;
constexpr float kPageHeight = 448.0f;
constexpr float kMaxRow = 40.0f;  // a row reaches the next one's top, at most this far
constexpr float kSameRow = 1.0f;  // tops this close share a row, side by side
// page: active
constexpr uint32_t kActive = 0x3D;
constexpr uint32_t kMaxWidgets = (kPageWidgetCount - kPageWidgets) / 4;
// widget: the offset drawn from its place, focusable when selectable and shown
constexpr uint32_t kOffsetX = 0x58;
constexpr uint32_t kOffsetY = 0x5C;
constexpr uint32_t kSelectable = 0xD8;
// a slider has no place of its own: its linked widgets, its title first
constexpr uint32_t kSlider = 0xF;
constexpr uint32_t kSliderLinks = 0x114;
constexpr uint32_t kSliderLinkCount = 0x118;

// the front end's books (story and journal): their pages, drawn into a 512 x 256 texture in their own units, rows
// running down it unless FlipVertical; an engine texture's D3D texture
constexpr uint32_t kBookPages = 0x234;
constexpr uint32_t kBookPageCount = 0x238;
constexpr uint32_t kBookFlip = 0x210;
constexpr uint32_t kBookTexture = 0x254;
constexpr uint32_t kTextureSource = 0x18;
constexpr uint32_t kSourceD3D = 0x8;
constexpr float kBookWidth = 512.0f;
constexpr float kBookHeight = 256.0f;
constexpr size_t kMaxBooks = 4;
// a book not drawn for this long may be freed: the front end's go when a level loads
constexpr auto kBookShown = std::chrono::milliseconds(250);
struct Book {
    uint32_t book;
    std::chrono::steady_clock::time_point drawn;
};
std::vector<Book> g_books;

void UpdateBookDrawn(uint32_t book) {
    auto now = std::chrono::steady_clock::now();
    auto it = std::find_if(g_books.begin(), g_books.end(), [&](const Book& b) { return b.book == book; });
    if (it != g_books.end()) {
        it->drawn = now;
        return;
    }
    if (g_books.size() == kMaxBooks) g_books.erase(g_books.begin());
    g_books.push_back({book, now});
}

// the book the page is on, 0 for none
uint32_t BookOf(uint32_t page) {
    auto now = std::chrono::steady_clock::now();
    for (const Book& b : g_books) {
        if (now - b.drawn > kBookShown) continue;
        uint32_t book = b.book;
        uint32_t pages = R32(book + kBookPages);
        uint32_t count = G<uint16_t>(book + kBookPageCount);
        for (uint32_t i = 0; pages && i < count; i++) {
            if (R32(pages + i * 4) == page) return book;
        }
    }
    return 0;
}

struct Row {
    int index;
    float left;
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

// the widget that places the row: a slider's title, else the widget itself
uint32_t Placed(uint32_t widget) {
    uint32_t links = R32(widget + kSliderLinks);
    bool slider = R32(widget + kWidgetKind) == kSlider && links && G<uint16_t>(widget + kSliderLinkCount);
    return slider ? R32(links) : widget;
}

std::vector<Row> Rows(uint32_t page) {
    std::vector<Row> rows;
    uint32_t count = std::min(R32(page + kPageWidgetCount), kMaxWidgets);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t widget = R32(page + kPageWidgets + i * 4);
        if (!widget || !G<uint8_t>(widget + kSelectable) || !G<uint8_t>(widget + kWidgetShown)) continue;
        uint32_t placed = Placed(widget);
        float left = G<float>(placed + kLeft) + G<float>(placed + kOffsetX);
        float top = G<float>(placed + kTop) + G<float>(placed + kOffsetY);
        rows.push_back({int(i), left, top});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.top < b.top; });
    return rows;
}

// the next row's top below this one's, at most kMaxRow down
float RowBottom(const std::vector<Row>& rows, size_t r) {
    for (size_t n = r + 1; n < rows.size(); n++) {
        if (rows[n].top > rows[r].top + kSameRow) return std::min(rows[r].top + kMaxRow, rows[n].top);
    }
    return rows[r].top + kMaxRow;
}

// the widget whose row the point is in, the nearest in x of a row side by side, -1 for none
int Under(uint32_t page, float x, float y) {
    std::vector<Row> rows = Rows(page);
    int under = -1;
    float nearest = 0.0f;
    for (size_t r = 0; r < rows.size(); r++) {
        if (y < rows[r].top || y >= RowBottom(rows, r)) continue;
        float dx = std::abs(x - rows[r].left);
        if (under < 0 || dx < nearest) {
            under = rows[r].index;
            nearest = dx;
        }
    }
    return under;
}

// the cursor's place on the page, false off it; a book page's from the renderer, which follows the cursor
bool CursorOnPage(uint32_t page, float& x, float& y) {
    if (!input::MenuCursorAt(x, y)) return false;
    x *= kPageWidth;
    y *= kPageHeight;
    uint32_t book = BookOf(page);
    if (!book) return true;
    float v = 0.0f;
    if (!xdk::Picked(x, v)) return false;
    x *= kBookWidth;
    y = G<uint8_t>(book + kBookFlip) ? (1.0f - v) * kBookHeight : v * kBookHeight;
    return true;
}

void UpdateHover(X86Context& ctx, uint32_t page) {
    float x = 0.0f;
    float y = 0.0f;
    if (!CursorOnPage(page, x, y) || !UpdateSeen(page, x, y)) return;
    int hovered = Under(page, x, y);
    int focused = int(R32(page + kPageFocused));
    if (hovered < 0 || hovered == focused) return;
    if (focused >= 0 && uint32_t(focused) < R32(page + kPageWidgetCount)) {
        CallSlot(ctx, R32(page + kPageWidgets + uint32_t(focused) * 4), kUnfocus, {});
    }
    CallSlot(ctx, R32(page + kPageWidgets + uint32_t(hovered) * 4), kFocus, {0});
    W32(page + kPageFocused, uint32_t(hovered));
}

// the save popup's movie, loaded at +0x28; a movie's player, the player's root object and movie, the movie's height
constexpr hle::Va kPopupMovie = SYM(PopupMovie);
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
constexpr hle::Va kGetGlobal = SYM(FlashGetVariable);
constexpr hle::Va kSetGlobal = SYM(SetGlobal);
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

// a movie's definition and root object, 0 without a player
struct MovieParts {
    uint32_t def = 0;
    uint32_t root = 0;
};

MovieParts PartsOf(uint32_t movie) {
    uint32_t player = R32(movie + kMoviePlayer);
    if (!player) return {};
    return {R32(player + kPlayerMovie), R32(player + kPlayerRoot)};
}

// the row under the cursor, nearest when past the ends
int RowUnder(uint32_t def, const MovieRows& rows, float y) {
    float scale = G<float>(rows.clip + kWorldScaleY);
    float row0 = float(rows.first) * scale + G<float>(rows.clip + kWorldY);
    float step = float(rows.line) * scale;
    float cursor = y * G<float>(def + kMovieHeight);
    return std::clamp(int(std::lround((cursor - row0) / step)), 0, rows.count - 1);
}

void Select(X86Context& ctx, uint32_t movie, uint32_t selection, int row) {
    ctx.ecx = movie;
    CallGuest(ctx, kSetGlobal, {selection, uint32_t(row)});
}

void UpdatePopupHover(X86Context& ctx) {
    static const uint32_t selection = GuestString("currentSelection");
    static const uint32_t top = GuestString("selectionVPos");
    static const uint32_t line = GuestString("menuLineOffset");
    uint32_t movie = R32(kPopupMovie);
    float x = 0.0f;
    float y = 0.0f;
    if (!movie || !G<uint8_t>(movie + kMovieLoaded) || !input::MenuCursorAt(x, y) || !UpdateSeen(movie, x, y)) return;
    MovieParts parts = PartsOf(movie);
    uint32_t screens = parts.root ? ChildNamed(parts.root, "theScreens") : 0;
    if (!parts.def || !screens) return;
    int current = 0;
    int first = 0;
    int offset = 0;
    bool found = GetGlobal(ctx, movie, selection, current) && GetGlobal(ctx, movie, top, first);
    if (!found || !GetGlobal(ctx, movie, line, offset) || offset == 0) return;
    int hovered = RowUnder(parts.def, {screens, first, offset, kPopupChoices}, y);
    if (hovered != current) Select(ctx, movie, selection, hovered);
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
    MovieParts parts = PartsOf(movie);
    uint32_t slots = parts.root ? ParentOfChild(parts.root, "slot0") : 0;
    if (!parts.def || !slots) return;
    int current = 0;
    int first = 0;
    int offset = 0;
    int max = 0;
    bool found = GetGlobal(ctx, movie, selection, current) && GetGlobal(ctx, movie, top, first);
    found = found && GetGlobal(ctx, movie, line, offset) && GetGlobal(ctx, movie, last, max);
    if (!found || offset == 0 || max < 0) return;
    int hovered = RowUnder(parts.def, {slots, first, offset, max + 1}, y);
    if (hovered != current) Select(ctx, movie, selection, hovered);
}

// The level's end menu: rewardMenu's slots, Continue, Replay and Quit down the page (no Quit in noQuit), curMenSel
// their row. Only that movie sets secondMenuOn, the confirmation after it.
constexpr const char* kRewardSlots[] = {"slot0", "slot1", "slot2"};
constexpr float kSlotTextMiddle = 10.5f;  // a slot's text middle below its origin, in its own units

void UpdateRewardMenuHover(X86Context& ctx, uint32_t movie) {
    static const uint32_t shown = GuestString("menuOn");
    static const uint32_t second = GuestString("secondMenuOn");
    static const uint32_t third = GuestString("thirdMenuOn");
    static const uint32_t no_quit = GuestString("noQuit");
    static const uint32_t selection = GuestString("curMenSel");
    float x = 0.0f;
    float y = 0.0f;
    int on = 0;
    int confirming = 0;
    int confirming_again = 0;
    if (!game::InLoadShow() || !G<uint8_t>(movie + kMovieLoaded) || !GetGlobal(ctx, movie, second, confirming)) return;
    GetGlobal(ctx, movie, third, confirming_again);
    if (!GetGlobal(ctx, movie, shown, on) || !on || confirming || confirming_again) return;
    if (!input::MenuCursorAt(x, y) || !UpdateSeen(movie, x, y)) return;
    MovieParts parts = PartsOf(movie);
    uint32_t holder = parts.root ? ParentOfChild(parts.root, "rewardMenu") : 0;
    uint32_t menu = holder ? ChildNamed(holder, "rewardMenu") : 0;
    int current = 0;
    int quit_gone = 0;
    if (!parts.def || !menu || !GetGlobal(ctx, movie, selection, current)) return;
    GetGlobal(ctx, movie, no_quit, quit_gone);
    int rows = quit_gone ? 2 : 3;
    float cursor = y * G<float>(parts.def + kMovieHeight);
    int hovered = 0;
    float nearest = std::numeric_limits<float>::max();
    for (int row = 0; row < rows; row++) {
        uint32_t slot = ChildNamed(menu, kRewardSlots[row]);
        if (!slot) return;
        float middle = G<float>(slot + kWorldY) + kSlotTextMiddle * G<float>(slot + kWorldScaleY);
        float distance = std::abs(cursor - middle);
        if (distance < nearest) {
            nearest = distance;
            hovered = row;
        }
    }
    if (hovered != current) Select(ctx, movie, selection, hovered);
}

}  // namespace

// a page's update, thiscall(page, ?) ret 4, before its input
X86_EXTERN_FUNC(__imp__sub_PageUpdate);
X86_FUNC(sub_PageUpdate) {
    uint32_t page = ctx.ecx;
    if (G<uint8_t>(page + kActive)) {
        UpdateHover(ctx, page);
        game::UpdateStartPrompt(page);
    }
    ctx.ecx = page;
    __imp__sub_PageUpdate(ctx, base);
    game::UpdateExitButton(page);
}

// the book's page into its texture, thiscall(book, texture, page) ret 8, before the book is drawn with it
X86_EXTERN_FUNC(__imp__sub_BookPageTexture);
X86_FUNC(sub_BookPageTexture) {
    UpdateBookDrawn(ctx.ecx);
    uint32_t texture = R32(ctx.ecx + kBookTexture);
    uint32_t source = texture ? R32(texture + kTextureSource) : 0;
    float x = 0.0f;
    float y = 0.0f;
    bool at = input::MenuCursorAt(x, y);
    xdk::SetPick(at && source ? R32(source + kSourceD3D) : 0, x, y);
    __imp__sub_BookPageTexture(ctx, base);
}

// the popups' update, fastcall(popup): boot, load and save, each through its own
X86_EXTERN_FUNC(__imp__sub_PopupsUpdate);
X86_FUNC(sub_PopupsUpdate) {
    uint32_t popup = ctx.ecx;
    UpdatePopupHover(ctx);
    ctx.ecx = popup;
    __imp__sub_PopupsUpdate(ctx, base);
}

// a Flash movie's update, thiscall(movie, seconds) ret 4: its script reads the buttons
X86_EXTERN_FUNC(__imp__sub_FlashMovieUpdate);
X86_FUNC(sub_FlashMovieUpdate) {
    uint32_t movie = ctx.ecx;
    UpdateLevelMenuHover(ctx, movie);
    UpdateRewardMenuHover(ctx, movie);
    ctx.ecx = movie;
    __imp__sub_FlashMovieUpdate(ctx, base);
}
