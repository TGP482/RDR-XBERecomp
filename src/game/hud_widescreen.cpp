// Widescreen HUD: the game's 4:3 2D keeps its shape in the middle of the screen (d3d/d3d8.cpp), and each HUD element
// goes to the side of the screen it sat on last frame, so the HUD keeps to the screen's edges. In the front end only the
// button prompts and the Flash movies (title, shop, upgrades) narrow, in the middle; the legal screen's picture
// narrows to 4:3 anywhere.
#include "d3d/inline_shift.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kFrontendButtonsVtable = 0x3D3500;  // swuiPageFrontendButtons, the button prompts
constexpr float kSide = 1.0f / 3;  // clip space: an element centred past this goes to its side
constexpr float kFullWidth = 0.98f;
constexpr float kNarrow = 0.75f;  // 4:3 over 16:9

struct Element {
    int side = 0;
};

int g_depth = 0;

int SideOf(const xdk::ScreenSpan& span) {
    if (span.x0 <= -kFullWidth && span.x1 >= kFullWidth) return 0;
    float middle = (span.x0 + span.x1) / 2;
    if (middle < -kSide) return -1;
    if (middle > kSide) return 1;
    return 0;
}

void BeginElement(const Element& e) {
    if (g_depth++ == 0) xdk::BeginInlineSide(e.side);
}

void EndElement(Element& e) {
    if (--g_depth > 0) return;
    xdk::ScreenSpan span = xdk::EndInlineSide();
    if (span.drawn) e.side = SideOf(span);
}

void BeginMiddle() {
    if (g_depth++ == 0) xdk::BeginInlineSide(0);
}

void EndMiddle() {
    if (--g_depth == 0) xdk::EndInlineSide();
}

}  // namespace

// the elements the HUD's draw (sub_001DAB00) calls
#define HUD_ELEMENT(name)              \
    X86_EXTERN_FUNC(__imp__##name);    \
    X86_FUNC(name) {                   \
        static Element element;        \
        BeginElement(element);         \
        __imp__##name(ctx, base);      \
        EndElement(element);           \
    }

HUD_ELEMENT(sub_001D5000)
HUD_ELEMENT(sub_001D60E0)
HUD_ELEMENT(sub_001D75A0)
HUD_ELEMENT(sub_001D8D60)
HUD_ELEMENT(sub_001D9550)
HUD_ELEMENT(sub_001D96D0)
HUD_ELEMENT(sub_001DE080)
HUD_ELEMENT(sub_001DEF00)
HUD_ELEMENT(sub_001E42D0)
HUD_ELEMENT(sub_001E54D0)
HUD_ELEMENT(sub_001E7FB0)

// the letterbox, thiscall(hud): its bars and torn edges already span the screen, kept
X86_EXTERN_FUNC(__imp__sub_001D7120);
X86_FUNC(sub_001D7120) {
    xdk::BeginInlineKeep();
    __imp__sub_001D7120(ctx, base);
    xdk::EndInlineKeep();
}

// the lock reticle, thiscall(reticle): centred on its target in the 3D view, kept
X86_EXTERN_FUNC(__imp__sub_001E2330);
X86_FUNC(sub_001E2330) {
    xdk::BeginInlineKeep();
    __imp__sub_001E2330(ctx, base);
    xdk::EndInlineKeep();
}

// the journal's draw over the 3D book, thiscall(page): its widgets and Flash already fit the book, kept
X86_EXTERN_FUNC(__imp__sub_00037970);
X86_FUNC(sub_00037970) {
    xdk::BeginInlineKeep();
    __imp__sub_00037970(ctx, base);
    xdk::EndInlineKeep();
}

// a swui page's widgets, thiscall(page)
X86_EXTERN_FUNC(__imp__sub_00019CC0);
X86_FUNC(sub_00019CC0) {
    bool prompts = R32(ctx.ecx) == kFrontendButtonsVtable;
    if (prompts) BeginMiddle();
    __imp__sub_00019CC0(ctx, base);
    if (prompts) EndMiddle();
}

// a Flash movie's draw, thiscall(movie)
X86_EXTERN_FUNC(__imp__sub_000188A0);
X86_FUNC(sub_000188A0) {
    BeginMiddle();
    __imp__sub_000188A0(ctx, base);
    EndMiddle();
}

// the legal screen's picture (rdrlogoshot), 4:3, drawn over the whole screen
X86_EXTERN_FUNC(__imp__sub_0005EB40);
X86_FUNC(sub_0005EB40) {
    if (g_depth > 0) {
        __imp__sub_0005EB40(ctx, base);
        return;
    }
    xdk::BeginInlineSide(0, xdk::Fit::Whole);
    __imp__sub_0005EB40(ctx, base);
    xdk::EndInlineSide();
}

// the tutorial's controller, thiscall(controller), vtable 0x3F5850 slot 6: the game keeps its shape on 16:9 but lays
// it out from the middle, so it moves against its side; the side comes from where it would sit on 4:3
X86_EXTERN_FUNC(__imp__sub_001E03C0);
X86_FUNC(sub_001E03C0) {
    static Element controller;
    if (g_depth > 0) {
        __imp__sub_001E03C0(ctx, base);
        return;
    }
    xdk::BeginInlineSide(controller.side, xdk::Fit::Shift);
    __imp__sub_001E03C0(ctx, base);
    xdk::ScreenSpan span = xdk::EndInlineSide();
    span.x0 /= kNarrow;
    span.x1 /= kNarrow;
    if (span.drawn) controller.side = SideOf(span);
}
