// PC prompts in the Flash movies (title, shop, upgrades): a button there is a picture, a movie clip named for it,
// laid over the PS2 button's picture. While the keyboard and mouse are in use both pictures are left out and the key
// drawn in the game's font in their place, "(ENTER)", ending where the picture did, in the colour and line of the text
// it labels when there is one, else as tall as the picture in white.
#include <algorithm>
#include <bit>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/input.h"
#include "game/font.h"
#include "xdk/hle.h"

using namespace kernel;
using namespace game_font;

namespace {

struct FlashButton {
    const char* name;
    input::PromptButton button;
};
constexpr FlashButton kFlashButtons[] = {
    {"acceptMainXbox", input::kPromptA},
    {"canXbox", input::kPromptB},
    {"xButtonXbox", input::kPromptX},
    {"yButtonXbox", input::kPromptY},
};

// display object: its character, world matrix (a b c d x y), world colour multiply (64 is one) and its own tint,
// flags, first child, instance name
constexpr uint32_t kObjectSize = 0x70;
constexpr uint32_t kCharacter = 0x4;
constexpr uint32_t kWorld = 0x20;
constexpr uint32_t kWorldAlpha = 0x43;
constexpr uint32_t kTintAlpha = 0x4F;
constexpr uint32_t kFlags = 0x5C;
constexpr uint32_t kFirstChild = 0x64;
constexpr uint32_t kName = 0x6C;
constexpr uint8_t kVisible = 0x20;
constexpr int kOne = 64;
// shape: its vtable and draw commands, shorts: 0 ends, 1 lines and 2 triangles (a count, then x y in twips), 3 has
// two more, 4 and 5 one
constexpr uint32_t kShapeVtable = 0x3EBB44;
constexpr uint32_t kShapeCommands = 0x10;
constexpr float kTwip = 0.05f;
// button: its vtable, record count and records (state mask, matrix, colour, character), drawn up when not focused
constexpr uint32_t kButtonVtable = 0x3EBB20;
constexpr uint32_t kButtonRecordCount = 0x9;
constexpr uint32_t kButtonRecords = 0xC;
constexpr uint32_t kRecordSize = 0x28;
constexpr uint32_t kRecordMatrix = 0x4;
constexpr uint32_t kRecordCharacter = 0x24;
constexpr uint8_t kUpState = 1;
// text: the next glyph vertex, its screen x and y first; the colour the glyphs draw in, with the object's tint
constexpr uint32_t kTextVertex = 0x6845F8;
constexpr uint32_t kGlyphColour = 0x684610;
// the movie being drawn: its pixels per unit across and down
constexpr uint32_t kScaleX = 0x6855C4;
constexpr uint32_t kScaleY = 0x6855E8;
constexpr size_t kMaxChars = 31;

struct Box {
    float x0 = 1e9f;
    float y0 = 1e9f;
    float x1 = -1e9f;
    float y1 = -1e9f;

    void Add(float x, float y) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
    }
    bool Empty() const { return x1 <= x0 || y1 <= y0; }
    void Add(const Box& b) {
        if (b.Empty()) return;
        Add(b.x0, b.y0);
        Add(b.x1, b.y1);
    }
};

struct FlashKey {
    Box box;
    std::string text;
    uint32_t colour;
};

// a text's glyphs on screen
struct FlashText {
    Box box;
    uint32_t colour;
};
std::vector<FlashText> g_texts;  // the movie's this frame
Box* g_glyphs = nullptr;         // the text drawing
Box* g_picture = nullptr;  // the picture being left out
std::vector<FlashKey> g_keys;
// each movie's pictures last frame: the shapes and buttons under them are the PS2 pictures
std::unordered_map<uint32_t, std::vector<Box>> g_pictures;
const std::vector<Box>* g_covered = nullptr;

const FlashButton* ButtonNamed(uint32_t name) {
    if (!name) return nullptr;
    const char* s = static_cast<const char*>(HostPtr(name));
    for (const FlashButton& b : kFlashButtons) {
        if (std::strcmp(s, b.name) == 0) return &b;
    }
    return nullptr;
}

// an object with no children draws its character; given an empty child it only takes its place
uint32_t EmptyChild() {
    static const uint32_t child = [] {
        uint32_t va = mem::AllocSystem(kObjectSize);
        std::memset(HostPtr(va), 0, kObjectSize);
        return va;
    }();
    return child;
}

// a 2D matrix as a b c d x y: x' = a x + c y + x, y' = b x + d y + y
struct Matrix {
    float m[6];
};

Matrix ReadMatrix(uint32_t va) {
    Matrix r;
    for (uint32_t i = 0; i < 6; i++) r.m[i] = G<float>(va + i * 4);
    return r;
}

// the local matrix then the parent's
Matrix Concat(const Matrix& local, const Matrix& parent) {
    const float* l = local.m;
    const float* p = parent.m;
    return {{p[0] * l[0] + p[2] * l[1], p[1] * l[0] + p[3] * l[1], p[0] * l[2] + p[2] * l[3],
             p[1] * l[2] + p[3] * l[3], p[0] * l[4] + p[2] * l[5] + p[4], p[1] * l[4] + p[3] * l[5] + p[5]}};
}

// the characters the picture's box is measured from
bool Boxable(uint32_t object) {
    uint32_t character = R32(object + kCharacter);
    uint32_t vtable = character ? R32(character) : 0;
    return vtable == kShapeVtable || vtable == kButtonVtable;
}

void AddShape(uint32_t shape, const Matrix& world, Box& box) {
    const float* m = world.m;
    float sx = G<float>(kScaleX);
    float sy = G<float>(kScaleY);
    uint32_t at = R32(shape + kShapeCommands);
    for (;;) {
        int16_t command = G<int16_t>(at);
        at += 2;
        if (command == 0) return;
        if (command == 3) at += 4;
        if (command == 4 || command == 5) at += 2;
        if (command != 1 && command != 2) continue;
        int count = G<int16_t>(at);
        at += 2;
        for (int i = 0; i < count; i++, at += 4) {
            float x = float(G<int16_t>(at)) * kTwip;
            float y = float(G<int16_t>(at + 2)) * kTwip;
            box.Add((m[0] * x + m[2] * y + m[4]) * sx, (m[1] * x + m[3] * y + m[5]) * sy);
        }
    }
}

void AddCharacter(uint32_t character, const Matrix& world, Box& box) {
    if (R32(character) == kShapeVtable) AddShape(character, world, box);
    if (R32(character) != kButtonVtable) return;
    uint32_t record = R32(character + kButtonRecords);
    for (uint32_t i = 0; i < G<uint8_t>(character + kButtonRecordCount); i++, record += kRecordSize) {
        uint32_t shape = R32(record + kRecordCharacter);
        if (!(G<uint8_t>(record) & kUpState) || !shape || R32(shape) != kShapeVtable) continue;
        AddShape(shape, Concat(ReadMatrix(record + kRecordMatrix), world), box);
    }
}

bool Covered(const Box& shape) {
    float x = (shape.x0 + shape.x1) / 2;
    float y = (shape.y0 + shape.y1) / 2;
    float area = (shape.x1 - shape.x0) * (shape.y1 - shape.y0);
    for (const Box& b : *g_covered) {
        bool inside = x > b.x0 && x < b.x1 && y > b.y0 && y < b.y1;
        if (inside && area < (b.x1 - b.x0) * (b.y1 - b.y0) * 4) return true;
    }
    return false;
}

// the text the key labels lies just past its picture, across its line; the key takes its colour and glyphs' height
void FitToText(FlashKey& key) {
    float height = key.box.y1 - key.box.y0;
    for (const FlashText& t : g_texts) {
        bool after = t.box.x1 >= key.box.x1 - height && t.box.x0 <= key.box.x1 + height * 2;
        bool level = t.box.y1 > key.box.y0 && t.box.y0 < key.box.y1;
        if (!after || !level) continue;
        key.colour = (key.colour & 0xFF000000u) | (t.colour & 0xFFFFFFu);
        key.box.y0 = t.box.y0;
        key.box.y1 = t.box.y1;
        return;
    }
}

uint32_t Alpha(uint32_t object) {
    int multiply = std::clamp(int(G<int8_t>(object + kWorldAlpha)) * 255 / kOne, 0, 255);
    return uint32_t(multiply) * G<uint8_t>(object + kTintAlpha) / 255;
}

void DrawKey(X86Context& ctx, uint32_t font, const FlashKey& key) {
    static const uint32_t block = mem::AllocSystem(kLayoutSize + (kMaxChars + 1) * 2);
    uint32_t layout = block;
    uint32_t text = block + kLayoutSize;
    size_t n = std::min(key.text.size(), kMaxChars);
    for (size_t i = 0; i < n; i++) W16(text + uint32_t(i) * 2, uint8_t(key.text[i]));
    W16(text + uint32_t(n) * 2, 0);
    ctx.ecx = layout;
    CallGuest(ctx, kInitLayout, {});
    W32(layout + kLayoutAlign, R32(layout + kLayoutAlign) | kAlignRight);
    W32(layout + kLayoutRight, 0);
    W32(layout + kLayoutColour, key.colour);
    float scale = (key.box.y1 - key.box.y0) / float(R32(font + kLineHeight));
    uint32_t x = std::bit_cast<uint32_t>(key.box.x1);
    uint32_t y = std::bit_cast<uint32_t>(key.box.y0);
    uint32_t s = std::bit_cast<uint32_t>(scale);
    ctx.ecx = font;
    CallGuest(ctx, kDrawText, {x, y, text, s, s, layout, 0, 0});
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_001AC840);

namespace {

// the object's place and colour without drawing it, and its character's box
Box Place(X86Context& ctx, uint8_t* base, uint32_t object) {
    W32(object + kFirstChild, EmptyChild());
    __imp__sub_001AC840(ctx, base);
    W32(object + kFirstChild, 0);
    Box box;
    AddCharacter(R32(object + kCharacter), ReadMatrix(object + kWorld), box);
    return box;
}

}  // namespace

// a display object's draw, thiscall(object, parent's transform) ret 4: its world matrix and colour, then its children
// or its character
X86_FUNC(sub_001AC840) {
    uint32_t object = ctx.ecx;
    uint32_t esp = ctx.esp;
    bool visible = G<uint8_t>(object + kFlags) & kVisible;
    bool boxable = visible && !R32(object + kFirstChild) && Boxable(object);
    bool keys = input::KeyboardActive();
    if (g_picture) {
        if (!boxable) __imp__sub_001AC840(ctx, base);
        else g_picture->Add(Place(ctx, base, object));
        return;
    }
    const FlashButton* button = visible && keys ? ButtonNamed(R32(object + kName)) : nullptr;
    std::string key = button ? input::PromptKey(button->button) : std::string();
    if (!key.empty()) {
        Box box;
        g_picture = &box;
        if (!boxable) __imp__sub_001AC840(ctx, base);
        else box.Add(Place(ctx, base, object));
        g_picture = nullptr;
        uint32_t alpha = Alpha(object);
        if (!box.Empty() && alpha) g_keys.push_back({box, "(" + key + ")", alpha << 24 | 0xFFFFFFu});
        return;
    }
    if (boxable && keys && g_covered && !g_covered->empty()) {
        if (Covered(Place(ctx, base, object))) return;
        ctx.esp = esp;
        ctx.ecx = object;
    }
    __imp__sub_001AC840(ctx, base);
}

// a Flash movie's player draw, thiscall(player), inside the movie's draw states: the keys go over the movie
X86_EXTERN_FUNC(__imp__sub_001ABEC0);
X86_FUNC(sub_001ABEC0) {
    std::vector<Box>& pictures = g_pictures[ctx.ecx];
    g_covered = &pictures;
    g_texts.clear();
    __imp__sub_001ABEC0(ctx, base);
    g_covered = nullptr;
    pictures.clear();
    for (const FlashKey& key : g_keys) pictures.push_back(key.box);
    if (g_keys.empty()) return;
    uint32_t font = FindGameFont(ctx);
    if (font && R32(font + kLineHeight)) {
        for (FlashKey& key : g_keys) {
            FitToText(key);
            DrawKey(ctx, font, key);
        }
    }
    g_keys.clear();
}

// a text's glyphs, drawn between the two, kept for the keys beside them
static Box g_glyph_box;

static void BeginText() {
    g_glyph_box = {};
    g_glyphs = g_covered ? &g_glyph_box : nullptr;
}

static void EndText() {
    if (g_glyphs && !g_glyph_box.Empty()) g_texts.push_back({g_glyph_box, R32(kGlyphColour)});
    g_glyphs = nullptr;
}

// a static text's draw, thiscall(text, place, state) ret 8
X86_EXTERN_FUNC(__imp__sub_001AE2C0);
X86_FUNC(sub_001AE2C0) {
    BeginText();
    __imp__sub_001AE2C0(ctx, base);
    EndText();
}

// an edit text's draw, thiscall(text, place, state) ret 8
X86_EXTERN_FUNC(__imp__sub_001AE680);
X86_FUNC(sub_001AE680) {
    BeginText();
    __imp__sub_001AE680(ctx, base);
    EndText();
}

// a glyph vertex, cdecl(x, y, scale), written where the vertex pointer was
X86_EXTERN_FUNC(__imp__sub_001ADFA0);
X86_FUNC(sub_001ADFA0) {
    uint32_t vertex = R32(kTextVertex);
    __imp__sub_001ADFA0(ctx, base);
    if (g_glyphs) g_glyphs->Add(G<float>(vertex), G<float>(vertex + 4));
}
