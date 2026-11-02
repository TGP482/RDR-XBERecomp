// PC prompts: the game's button icons are characters of its fonts in the text, drawn by the font's draw. While the
// keyboard and mouse are in use each one is drawn as its key instead, "Press (R) to reload.", in every language. An
// icon drawn alone (the prompt bar's) grows to its left, so the text after it keeps its place.
#include <algorithm>
#include <iterator>
#include <string>

#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

struct PromptGlyph {
    char16_t glyph;
    input::PromptButton button;
};
// as the game's own text uses them: A joins, B is melee, X reloads, Y sneaks, LT keeps the guns drawn, RT fires
constexpr PromptGlyph kGlyphs[] = {
    {u'\u00BA', input::kPromptA},           {u'\u00BE', input::kPromptB},
    {u'\u00BC', input::kPromptX},           {u'\u00BD', input::kPromptY},
    {u'\u2020', input::kPromptLeftTrigger}, {u'\u2021', input::kPromptRightTrigger},
};
constexpr size_t kMaxChars = 1024;
constexpr uint32_t kBuffers = 8;  // texts a frame may still hold while later ones draw
// a font's measure, thiscall(font, width out, height out, text, scale x, scale y, layout) ret 0x18; a layout's
// alignment bits: 1 centred, 4 right
constexpr uint32_t kMeasure = 0x56040;
constexpr uint32_t kCentred = 1;
constexpr uint32_t kRight = 4;
bool g_measuring = false;  // our own measures see the game's text

std::u16string ReadText(uint32_t va) {
    std::u16string s;
    for (uint32_t i = 0; i < kMaxChars; i++) {
        char16_t c = char16_t(G<uint16_t>(va + i * 2));
        if (!c) break;
        s += c;
    }
    return s;
}

// the text with its icons as keys; false when it has none to change
bool WithKeys(const std::u16string& text, std::u16string& out) {
    bool changed = false;
    for (char16_t c : text) {
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
    CallGuest(ctx, kMeasure, {out, out + 4, text, d.scale_x, d.scale_y, d.layout});
    g_measuring = false;
    return int32_t(R32(out));
}

// how far left the key starts so it ends where the icon did
float LeftShift(X86Context& ctx, const DrawArgs& d, uint32_t keys) {
    uint32_t align = d.layout ? R32(d.layout) & (kCentred | kRight) : 0;
    if (align & kRight) return 0;
    float grow = float(Width(ctx, d, keys) - Width(ctx, d, d.text));
    return align & kCentred ? grow / 2 : grow;
}

}  // namespace

// a font's draw or measure, thiscall(font, x, y, text, scale x, scale y, layout, measure only, batch) ret 0x20
X86_EXTERN_FUNC(__imp__sub_00054A30);
X86_FUNC(sub_00054A30) {
    DrawArgs d{ctx.ecx, R32(ctx.esp + 0xC), R32(ctx.esp + 0x10), R32(ctx.esp + 0x14), R32(ctx.esp + 0x18)};
    std::u16string text = d.text && !g_measuring && input::KeyboardActive() ? ReadText(d.text) : std::u16string();
    std::u16string keys;
    if (!WithKeys(text, keys)) {
        __imp__sub_00054A30(ctx, base);
        return;
    }
    uint32_t replaced = WriteText(keys);
    bool measure = G<uint8_t>(ctx.esp + 0x1C) != 0;
    if (!measure && text.size() == 1 && IsGlyph(text[0])) {
        float shift = LeftShift(ctx, d, replaced);
        G<float>(ctx.esp + 4) -= shift;
        ctx.ecx = d.font;
    }
    W32(ctx.esp + 0xC, replaced);
    __imp__sub_00054A30(ctx, base);
}
