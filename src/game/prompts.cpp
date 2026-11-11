// PC prompts: the game's button icons are characters of its fonts in the text, drawn by the font's draw. While the
// keyboard and mouse are in use each one is drawn as its key instead, "Press (R) to reload.", in every language. An
// icon drawn alone grows to its left, so the text after it keeps its place; on a prompt bar's row of several keys the
// row lays out again, each key starting where its icon did. The Flash movies' text fields ("Press START" on the title)
// get the same.
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/input.h"
#include "d3d/linear_text.h"
#include "game/font.h"
#include "game/prompt_words.h"
#include "game/prompts.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

struct PromptGlyph {
    char16_t glyph;
    input::PromptButton button;
};
// as the game's own text uses them: A joins, B is melee, X sneaks, Y reloads, LT keeps the guns drawn, RT fires;
// the white and black ones stand where the PS2 had L2 and R2
constexpr PromptGlyph kGlyphs[] = {
    {u'\u00BA', input::kPromptA},           {u'\u00BE', input::kPromptB},
    {u'\u00BC', input::kPromptY},           {u'\u00BD', input::kPromptX},
    {u'\u2020', input::kPromptLeftTrigger}, {u'\u2021', input::kPromptRightTrigger},
    {u'\u2022', input::kPromptWhite},       {u'\u2023', input::kPromptBlack},
};
constexpr size_t kMaxChars = 1024;
constexpr uint32_t kBuffers = 8;  // texts a frame may still hold while later ones draw
// a layout's alignment bits: 1 centred, 4 right
constexpr uint32_t kCentred = 1;
constexpr uint32_t kRight = 4;
bool g_measuring = false;  // our own measures see the game's text
// a Flash text field's draw takes the text ActionScript set (copied into its buffer, thiscall(chunks, buffer, size)
// ret 8, 0 when none), else its variable's or its own (in ebp); the copy's return in that draw
constexpr hle::Va kFlashTextCopyReturn = SYM(FlashTextCopyReturn);
constexpr uint32_t kFlashTextBuffer = 0x100;

// a prompt bar's key: where the game drew its icon, and how much wider the key is
struct BarKey {
    float x;
    float y;
    float grow;
};
std::unordered_map<uint32_t, std::vector<BarKey>> g_bars;  // each bar's keys this frame
std::vector<BarKey> g_last_bar;                            // the drawing bar's keys last frame
std::vector<BarKey>* g_bar = nullptr;

std::u16string ReadText(uint32_t va) {
    std::u16string s;
    for (uint32_t i = 0; i < kMaxChars; i++) {
        char16_t c = char16_t(G<uint16_t>(va + i * 2));
        if (!c) break;
        s += c;
    }
    return s;
}

// the text with its controls named in words and its icons as keys; false when it has none to change
bool WithKeys(const std::u16string& text, std::u16string& out) {
    std::u16string words = text;
    bool changed = WithControlKeys(words);
    for (char16_t c : words) {
        std::string key;
        for (const PromptGlyph& g : kGlyphs) {
            if (g.glyph == c) key = input::PromptKey(g.button);
        }
        if (key.empty()) {
            out += c;
            continue;
        }
        out += u'(';
        out.append(key.begin(), key.end());
        out += u')';
        changed = true;
    }
    return changed;
}

uint32_t WriteText(const std::u16string& s) {
    static uint32_t buffers[kBuffers] = {};
    static uint32_t next = 0;
    uint32_t& buffer = buffers[next++ % kBuffers];
    if (!buffer) buffer = mem::AllocSystem(uint32_t(kMaxChars + 1) * 2);
    size_t n = std::min(s.size(), kMaxChars);
    for (size_t i = 0; i < n; i++) W16(buffer + uint32_t(i) * 2, s[i]);
    W16(buffer + uint32_t(n) * 2, 0);
    return buffer;
}

bool IsGlyph(char16_t c) {
    return std::any_of(std::begin(kGlyphs), std::end(kGlyphs), [c](const PromptGlyph& g) { return g.glyph == c; });
}

// draw's arguments past its return address
struct DrawArgs {
    uint32_t font;
    uint32_t text;
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t layout;
};

int32_t Width(X86Context& ctx, const DrawArgs& d, uint32_t text) {
    static const uint32_t out = mem::AllocSystem(8);
    ctx.ecx = d.font;
    g_measuring = true;
    CallGuest(ctx, game_font::kMeasureText, {out, out + 4, text, d.scale_x, d.scale_y, d.layout});
    g_measuring = false;
    return int32_t(R32(out));
}

// a Flash text field's own text, UTF-8, read and written within its draw's buffer
std::u16string ReadUtf8(uint32_t va) {
    std::u16string s;
    for (uint32_t i = 0; i < kFlashTextBuffer;) {
        uint8_t c = G<uint8_t>(va + i++);
        if (!c) break;
        if (c < 0x80) {
            s += char16_t(c);
            continue;
        }
        int more = c >= 0xE0 ? 2 : 1;
        uint32_t code = c & (more == 2 ? 0x0F : 0x1F);
        for (int k = 0; k < more; k++) code = code << 6 | (G<uint8_t>(va + i++) & 0x3F);
        s += char16_t(code);
    }
    return s;
}

void WriteUtf8(uint32_t va, const std::u16string& s) {
    std::string out;
    for (char16_t c : s) {
        if (c < 0x80) {
            out += char(c);
        } else if (c < 0x800) {
            out += char(0xC0 | c >> 6);
            out += char(0x80 | (c & 0x3F));
        } else {
            out += char(0xE0 | c >> 12);
            out += char(0x80 | (c >> 6 & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }
    size_t n = std::min(out.size(), size_t(kFlashTextBuffer - 1));
    while (n && (uint8_t(out[n]) & 0xC0) == 0x80) n--;
    for (size_t i = 0; i < n; i++) G<uint8_t>(va + uint32_t(i)) = uint8_t(out[i]);
    G<uint8_t>(va + uint32_t(n)) = 0;
}

// how far left a key grown by grow starts so it ends where the icon did
float LeftShift(const DrawArgs& d, float grow) {
    uint32_t align = d.layout ? R32(d.layout) & (kCentred | kRight) : 0;
    if (align & kRight) return 0;
    return align & kCentred ? grow / 2 : grow;
}

// On a bar's row of several keys a text moves right by how much the keys before it grew, the row's middle kept.
// False on a row of one key. Rows match exactly: the text's shadow, 2 pixels down, is a row of its own.
bool BarMove(float x, float y, float& move) {
    int keys = 0;
    float before = 0;
    float total = 0;
    for (const BarKey& k : g_last_bar) {
        if (std::abs(k.y - y) > 0.5f) continue;
        keys++;
        total += k.grow;
        if (k.x < x) before += k.grow;
    }
    move = before - total / 2;
    return keys > 1;
}

// the draw's x: a key drawn alone ends where its icon did, on a bar's row of keys it starts there
void PlaceText(X86Context& ctx, const DrawArgs& d, const std::u16string& text, uint32_t keys) {
    bool icon = keys && text.size() == 1 && IsGlyph(text[0]);
    if (!icon && !g_bar) return;
    float grow = icon ? float(Width(ctx, d, keys) - Width(ctx, d, d.text)) : 0;
    float& x = G<float>(ctx.esp + 4);
    float y = G<float>(ctx.esp + 8);
    if (icon && g_bar) g_bar->push_back({x, y, grow});
    float move = 0;
    if (g_bar && BarMove(x, y, move)) x += move + grow - LeftShift(d, grow);
    else x -= LeftShift(d, grow);
    ctx.ecx = d.font;
}

}  // namespace

// a font's draw or measure, thiscall(font, x, y, text, scale x, scale y, layout, measure only, batch) ret 0x20
X86_EXTERN_FUNC(__imp__sub_FontDraw);
X86_FUNC(sub_FontDraw) {
    DrawArgs d{ctx.ecx, R32(ctx.esp + 0xC), R32(ctx.esp + 0x10), R32(ctx.esp + 0x14), R32(ctx.esp + 0x18)};
    std::u16string text = d.text && !g_measuring && input::KeyboardActive() ? ReadText(d.text) : std::u16string();
    std::u16string keys;
    uint32_t replaced = WithKeys(text, keys) ? WriteText(keys) : 0;
    bool measure = G<uint8_t>(ctx.esp + 0x1C) != 0;
    if (!measure && !text.empty()) PlaceText(ctx, d, text, replaced);
    if (replaced) W32(ctx.esp + 0xC, replaced);
    // unbatched, a pixel font's text sets point sampling and draws at once (sub_00054A30)
    bool pixel = !G<uint8_t>(d.font + game_font::kSmooth) && !R32(ctx.esp + 0x20);
    xdk::SetLinearText(pixel);
    __imp__sub_FontDraw(ctx, base);
    xdk::SetLinearText(false);
}

void game::BeginPromptBar(uint32_t page) {
    g_bar = &g_bars[page];
    g_last_bar.clear();
    g_last_bar.swap(*g_bar);
}

void game::EndPromptBar() { g_bar = nullptr; }

// a Flash text field's copy of its ActionScript text; in a text field's draw it hands over the text shown, as keys
X86_EXTERN_FUNC(__imp__sub_FlashTextCopy);
X86_FUNC(sub_FlashTextCopy) {
    bool draw = R32(ctx.esp) == kFlashTextCopyReturn;
    uint32_t buffer = R32(ctx.esp + 4);
    uint32_t own = ctx.ebp;
    __imp__sub_FlashTextCopy(ctx, base);
    if (!draw || !input::KeyboardActive()) return;
    uint32_t text = ctx.eax ? buffer : own;
    std::u16string keys;
    if (!text || !WithKeys(ReadUtf8(text), keys)) return;
    WriteUtf8(buffer, keys);
    ctx.eax = buffer;
}
