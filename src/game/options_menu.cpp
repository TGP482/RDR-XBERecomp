// RECOMP SETTINGS, MOUSE AND KEYBOARD and CONTROLLER in the Options pages, the front end's and the pause menu's, after
// their own. Each opens a page listing its sections (the F10 panel's headings), each section its own page. The pages
// are SWUI scripts the recomp writes, read line by line as the game reads its own .swui files.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "app/menu.h"
#include "app/translations.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// page: its name, its lines read so far, its vtable's +0x34 after the last line; a line's tags, thiscall(page, line,
// length) ret 8
constexpr uint32_t kPageName = 0x1C;
constexpr uint32_t kPageLines = 0x448;
constexpr uint32_t kLoaded = 0x34;
constexpr uint32_t kReadLine = 0x1E990;
constexpr size_t kMaxLine = 255;
// the page manager picks a page's kind by its name: ours are made as this menu page is, then read as ours
constexpr uint32_t kMenuKindName = 0x3D2080;  // "gameoptions"
// the Options pages and their own buttons, ours after those; the buttons' rows, closer when they pass the last
struct OptionsPage {
    const char* name;
    int buttons;
};
constexpr OptionsPage kOptionsPages[] = {{"mainoptions", 2}, {"gameoptions", 3}};
constexpr int kOptionsTop = 130;
constexpr int kOptionsStep = 36;
constexpr int kSectionsTop = 135;
constexpr int kSectionsBottom = 295;  // the last section's top, its text clear of the prompts
constexpr int kSectionsStep = 30;
constexpr float kSectionsScale = 1.195f;
constexpr size_t kMaxLabel = 31;  // a button's label
// a page's widgets, their count, the focused one, the pad's pressed buttons; a widget's kind, 4 a combo; a combo's
// items, each first its text
constexpr uint32_t kWidgets = 0x40;
constexpr uint32_t kWidgetCount = 0x298;
constexpr uint32_t kFocused = 0x31C;
constexpr uint32_t kPressed = 0x308;
constexpr uint32_t kWidgetKind = 0x48;
constexpr uint32_t kCombo = 4;
constexpr uint32_t kComboLabel = 0x114;  // the drawn copy, 0x28 is the name parsed once
constexpr uint32_t kComboItems = 0x140;
constexpr size_t kItemTextMax = 0x27;
constexpr uint32_t kLeftArrowAlpha = 0x129C;
constexpr uint32_t kRightArrowAlpha = 0x12AC;
constexpr uint32_t kLeft = 0x4000;
constexpr uint32_t kRight = 0x8000;
constexpr uint32_t kUp = 0x1000;
constexpr uint32_t kDown = 0x2000;
constexpr uint32_t kConfirm = 0x1000000;
constexpr uint32_t kButtonX = 0x4000000;
// a section's rows: Player Options' place, spacing and font, closer and smaller past the rows that fit
constexpr int kRowsTop = 140;
constexpr int kRowsBottom = 320;
constexpr int kRowsStep = 35;
constexpr int kRowsFit = 20;
constexpr float kRowsScale = 0.75f;
// a longer section shows this many rows at a time, scrolling past its first and last
constexpr size_t kMaxShown = 12;
// a widget's focus(quiet) and unfocus slots
constexpr uint32_t kFocus = 0x10;
constexpr uint32_t kUnfocus = 0x14;

std::string g_loading;  // our page the manager is making
int g_buttons = 0;                // the Options page's buttons read
bool g_adding = false;            // our buttons going into the Options page
// the prompts row: Back and its text go this much right, the text this far from its icon at most
constexpr int kPromptsRow = 343;
constexpr int kBackShift = 25;
constexpr int kPromptText = 50;
struct Prompts {
    bool confirm = false;
    int back = 0;  // Back's icon's x
};
Prompts g_prompts;  // the page being read
std::vector<uint32_t> g_arrowless;  // the key bind rows' combos, drawn without arrows
size_t g_scroll = 0;                // the section row the first combo shows

constexpr const char* kAcronyms[] = {"FPS", "HDR", "FMV", "MSAA", "SMAA", "SSAO", "GTAO", "PNG", "DDS"};

bool Acronym(const std::string& text, size_t at) {
    size_t end = std::min(text.find(' ', at), text.size());
    for (const char* a : kAcronyms) {
        if (text.compare(at, end - at, a) == 0) return true;
    }
    return false;
}

// the game's language in latin 1, as the game shows text it has no string for; English in title case
std::string Label(const std::string& english, size_t max = kMaxLabel) {
    std::string out;
    if (const char16_t* t = app::Translation(english)) {
        for (; *t && out.size() < max; t++) out += *t < 0x100 ? char(*t) : '?';
        return out;
    }
    bool word_start = true;
    bool keep = false;  // an acronym's letters
    for (size_t i = 0; i < english.size(); i++) {
        char c = english[i];
        if (word_start) keep = Acronym(english, i);
        bool small_word = english.compare(i, 4, "AND ") == 0 && i > 0;
        if (!keep) c = word_start && !small_word ? char(std::toupper(uint8_t(c))) : char(std::tolower(uint8_t(c)));
        out += c;
        word_start = c == ' ' || c == '-' || c == '(';
    }
    return out.substr(0, max);
}

// a script line's text, its tags' separators taken out
std::string InScript(std::string text) {
    std::replace_if(text.begin(), text.end(), [](char c) { return c == ':' || c == '<' || c == '>'; }, ' ');
    return text;
}

std::string PageName(int page) { return "rcmp" + std::to_string(page); }
std::string SectionName(int page, size_t section) { return PageName(page) + "_" + std::to_string(section); }

std::string Button(int y, const std::string& label, const std::string& target) {
    char line[kMaxLine + 1];
    std::snprintf(line, sizeof(line), "<BUTTON:200,%d,0:%s:%s:0,0:640,448:1:-1:0>", y, label.c_str(), target.c_str());
    return line;
}

// the stock option pages' background and title
void AddTitle(std::vector<std::string>& lines, const std::string& title) {
    lines.push_back("<TEXPHASE:HUD>");  // as the stock option pages, loaded in the front end and in play
    lines.push_back("<IPU_BACKGROUND:0,0,0:0,0:options_bg_10k>");
    lines.push_back("<FONT:regulatorcondensed:SCALE:2,2:CENTER>");
    lines.push_back("<COLOR:1,1,1,1><COLORC:1,1,1,1>");
    lines.push_back("<TEXT:200,71,0:" + Label(title) + "><BASECOLOR:0.289,0.041,0,1>");
}

std::vector<std::string> CategoryScript(int page) {
    std::vector<std::string> lines;
    AddTitle(lines, app::PanelPageTitle(page));
    std::vector<std::string> sections = app::PanelSections(page);
    int gaps = std::max(int(sections.size()) - 1, 1);
    int step = std::min(kSectionsStep, (kSectionsBottom - kSectionsTop) / gaps);
    float scale = kSectionsScale * float(step) / float(kSectionsStep);
    char font[kMaxLine + 1];
    std::snprintf(font, sizeof(font), "<FONT:regulatorcondensed:SCALE:%.3f,%.3f:CENTER>", scale, scale);
    lines.push_back(font);
    lines.push_back("<COLOR:0.702,0.702,0.702,1><COLORC:0.771,0.771,0.771,0.500>");
    for (size_t s = 0; s < sections.size(); s++) {
        int y = kSectionsTop + step * int(s);
        lines.push_back(Button(y, Label(sections[s]), SectionName(page, s)));
    }
    lines.push_back("<FONT:berrylium>");
    lines.push_back("<TEXT:196,343,0:BUTTON_CONFIRM>");
    lines.push_back("<TEXT:327,343,0:BUTTON_BACK>");
    lines.push_back("<FONT:berrylium>");
    lines.push_back("<TEXT:232,343,0:Select>");
    lines.push_back("<TEXT:362,343,0:Back>");
    return lines;
}

// three prompts, smaller and further apart than two, the keys' names being wider than the console's buttons
void AddBindPrompts(std::vector<std::string>& lines) {
    lines.push_back("<FONT:berrylium:SCALE:0.800,0.800>");
    lines.push_back("<TEXT:115,343,0:BUTTON_CONFIRM><TEXT:300,343,0:BUTTON_ALT2_MISC><TEXT:520,343,0:BUTTON_BACK>");
    lines.push_back("<FONT:berrylium:SCALE:0.800,0.800>");
    lines.push_back("<TEXT:145,343,0:" + InScript(Label("CHANGE KEY")) + ">");
    lines.push_back("<TEXT:330,343,0:" + InScript(Label("ALTERNATE KEY")) + ">");
    lines.push_back("<TEXT:550,343,0:Back>");
}

// a section's rows as the Player Options page's combos, the first item their value (one alone is unselectable); the
// rows close up to fit, past kMaxShown they scroll
std::vector<std::string> SectionScript(int page, size_t section) {
    std::vector<std::string> lines;
    AddTitle(lines, app::PanelSections(page)[section]);
    std::vector<app::PanelRow> rows = app::PanelRows(page, section);
    size_t shown = std::min(rows.size(), kMaxShown);
    int step = std::min(kRowsStep, (kRowsBottom - kRowsTop) / std::max(int(shown) - 1, 1));
    float scale = std::min(kRowsScale, kRowsScale * float(step) / float(kRowsFit));
    char line[kMaxLine + 1];
    std::snprintf(line, sizeof(line), "<FONT:berrylium:SCALE:%.3f,%.3f>", scale, scale);
    lines.push_back(line);
    lines.push_back("<COLOR:0.699,0.699,0.699,1><COLORC:0.769,0.769,0.769,0.500>");
    for (size_t r = 0; r < shown; r++) {
        int y = kRowsTop + step * int(r);
        std::string label = InScript(Label(rows[r].label));
        std::snprintf(line, sizeof(line), "<COMBO:80,%d,0:%s:347,%d:200,%d:10>", y, label.c_str(), y + 4, step - 4);
        lines.push_back(line);
        lines.push_back("<COMBODATA:-:0><COMBODATA:-:1>");
    }
    bool binds = std::any_of(rows.begin(), rows.end(), [](const app::PanelRow& r) { return r.bind; });
    if (binds) {
        AddBindPrompts(lines);
        return lines;
    }
    lines.push_back("<FONT:berrylium>");
    lines.push_back("<TEXT:331,343,0:BUTTON_BACK>");
    lines.push_back("<FONT:berrylium>");
    lines.push_back("<TEXT:369,343,0:Back>");
    return lines;
}

// one of our section pages' page and section; false for any other page
bool SectionPage(const std::string& name, int& page, size_t& section) {
    for (page = 0; page < app::kPanelPages; page++) {
        size_t sections = app::PanelSections(page).size();
        for (section = 0; section < sections; section++) {
            if (name == SectionName(page, section)) return true;
        }
    }
    return false;
}

// one of our page names' script; false for any other page
bool Script(const std::string& name, std::vector<std::string>& lines) {
    for (int page = 0; page < app::kPanelPages; page++) {
        if (name != PageName(page)) continue;
        lines = CategoryScript(page);
        return true;
    }
    int page = 0;
    size_t section = 0;
    if (!SectionPage(name, page, section)) return false;
    lines = SectionScript(page, section);
    return true;
}

std::string ReadName(uint32_t va) { return static_cast<const char*>(HostPtr(va)); }

// an Options page's own buttons, 0 for any other page
int OwnButtons(uint32_t page) {
    std::string name = ReadName(page + kPageName);
    for (const OptionsPage& o : kOptionsPages) {
        if (name == o.name) return o.buttons;
    }
    return 0;
}

int OptionsRow(int own, int index) {
    int step = std::min(kOptionsStep, (kSectionsBottom - kOptionsTop) / (own + app::kPanelPages - 1));
    return kOptionsTop + step * index;
}

uint32_t WriteLine(const std::string& line) {
    static const uint32_t buffer = mem::AllocSystem(kMaxLine + 1);
    size_t n = std::min(line.size(), kMaxLine);
    std::memcpy(HostPtr(buffer), line.data(), n);
    G<uint8_t>(buffer + uint32_t(n)) = 0;
    return buffer;
}

uint32_t ReadLine(X86Context& ctx, uint32_t page, const std::string& line) {
    uint32_t va = WriteLine(line);
    ctx.ecx = page;
    return CallGuest(ctx, kReadLine, {va, uint32_t(std::min(line.size(), kMaxLine))});
}

// the page's combos, in their rows' order
std::vector<uint32_t> Combos(uint32_t page) {
    std::vector<uint32_t> out;
    for (uint32_t i = 0; i < R32(page + kWidgetCount); i++) {
        uint32_t widget = R32(page + kWidgets + i * 4);
        if (widget && R32(widget + kWidgetKind) == kCombo) out.push_back(widget);
    }
    return out;
}

// keys as the prompts show them, "(Q) (MMB)"; a bare name can be one of the game's strings, "D" shows "Done"
std::string KeysText(const std::string& keys) {
    if (keys == "PRESS A KEY") return Label(keys);
    std::string out;
    for (size_t start = 0; start <= keys.size();) {
        size_t comma = std::min(keys.find(", ", start), keys.size());
        std::string key = keys.substr(start, comma - start);
        if (key != "UNBOUND") out += (out.empty() ? "(" : " (") + key + ")";
        start = comma + 2;
    }
    return out.empty() ? Label("UNBOUND") : out;
}

// each row's value as its combo's one item; a section page's only
void ShowValues(uint32_t page) {
    int panel_page = 0;
    size_t section = 0;
    if (!SectionPage(ReadName(page + kPageName), panel_page, section)) return;
    std::vector<app::PanelRow> rows = app::PanelRows(panel_page, section);
    std::vector<uint32_t> combos = Combos(page);
    g_arrowless.clear();
    for (size_t i = 0; i < combos.size() && g_scroll + i < rows.size(); i++) {
        const app::PanelRow& row = rows[g_scroll + i];
        std::string label = Label(row.label);
        std::memcpy(HostPtr(combos[i] + kComboLabel), label.c_str(), label.size() + 1);
        std::string text = row.bind ? KeysText(row.value) : Label(row.value);
        text = text.substr(0, kItemTextMax);
        std::memcpy(HostPtr(combos[i] + kComboItems), text.c_str(), text.size() + 1);
        if (row.bind) g_arrowless.push_back(combos[i]);
    }
}

// the focused widget's place among the combos, -1 for none
int FocusedCombo(uint32_t page, const std::vector<uint32_t>& combos) {
    uint32_t focused = R32(page + kFocused);
    uint32_t widget = focused < R32(page + kWidgetCount) ? R32(page + kWidgets + focused * 4) : 0;
    auto it = std::find(combos.begin(), combos.end(), widget);
    return widget && it != combos.end() ? int(it - combos.begin()) : -1;
}

void CallSlot(X86Context& ctx, uint32_t widget, uint32_t slot, std::initializer_list<uint32_t> args) {
    ctx.ecx = widget;
    CallGuest(ctx, R32(R32(widget) + slot), args);
}

void UpdateFocus(X86Context& ctx, uint32_t page, uint32_t widget) {
    uint32_t focused = R32(page + kFocused);
    if (focused < R32(page + kWidgetCount)) CallSlot(ctx, R32(page + kWidgets + focused * 4), kUnfocus, {});
    CallSlot(ctx, widget, kFocus, {0});
    for (uint32_t i = 0; i < R32(page + kWidgetCount); i++) {
        if (R32(page + kWidgets + i * 4) == widget) W32(page + kFocused, i);
    }
}

// Up on the first combo and Down on the last scroll the rows while there are more; the page's own wrap around
// stands only at the ends, which show the other end
void UpdateScroll(X86Context& ctx, uint32_t page, const std::vector<uint32_t>& combos, int before, size_t rows) {
    size_t shown = combos.size();
    if (rows <= shown || before < 0) return;
    uint32_t pressed = R32(page + kPressed);
    bool first = before == 0;
    bool last = size_t(before) == shown - 1;
    if ((pressed & kUp) && first) {
        if (g_scroll == 0) {
            g_scroll = rows - shown;
            return;
        }
        g_scroll--;
        UpdateFocus(ctx, page, combos[0]);
    } else if ((pressed & kDown) && last) {
        if (g_scroll + shown == rows) {
            g_scroll = 0;
            return;
        }
        g_scroll++;
        UpdateFocus(ctx, page, combos[shown - 1]);
    }
}

// "<TEXT:x,343,0:BUTTON_BACK>" and the text after it moved right when the page has a confirm prompt before it; lines
// of more tags left as they are
std::string WithPrompt(const std::string& line) {
    int x = 0;
    int y = 0;
    bool one_tag = line.find('<', 1) == std::string::npos;
    if (!one_tag || std::sscanf(line.c_str(), "<TEXT:%d,%d,", &x, &y) != 2 || y != kPromptsRow) return line;
    if (line.find("BUTTON_CONFIRM") != std::string::npos) g_prompts.confirm = true;
    bool icon = g_prompts.confirm && line.find("BUTTON_BACK") != std::string::npos;
    if (icon) g_prompts.back = x;
    bool text = g_prompts.back && x > g_prompts.back && x <= g_prompts.back + kPromptText;
    if (!icon && !text) return line;
    size_t comma = line.find(',');
    return "<TEXT:" + std::to_string(x + kBackShift) + line.substr(comma);
}

// "<BUTTON:x,y,..." with another y
std::string WithRow(const std::string& line, int y) {
    size_t first = line.find(',');
    size_t second = first == std::string::npos ? first : line.find(',', first + 1);
    if (second == std::string::npos) return line;
    return line.substr(0, first + 1) + std::to_string(y) + line.substr(second);
}

}  // namespace

// a line of a page's script, thiscall(page, line, length) ret 8: an Options page's own buttons move up for ours, which
// follow its last; a page's Back prompt moves right of its confirm's, which the PC keys widen
X86_EXTERN_FUNC(__imp__sub_0001E990);
X86_FUNC(sub_0001E990) {
    uint32_t page = ctx.ecx;
    std::string line = static_cast<const char*>(HostPtr(R32(ctx.esp + 4)));
    if (g_adding) {
        __imp__sub_0001E990(ctx, base);
        return;
    }
    if (R32(page + kPageLines) == 1) {
        g_buttons = 0;
        g_prompts = {};
    }
    int own = OwnButtons(page);
    bool button = own && line.rfind("<BUTTON:", 0) == 0;
    std::string moved = button ? WithRow(line, OptionsRow(own, g_buttons++)) : WithPrompt(line);
    if (moved != line) {
        W32(ctx.esp + 4, WriteLine(moved));
        W32(ctx.esp + 8, uint32_t(std::min(moved.size(), kMaxLine)));
    }
    __imp__sub_0001E990(ctx, base);
    if (!button || g_buttons != own) return;
    uint32_t result = ctx.eax;
    g_adding = true;
    for (int p = 0; p < app::kPanelPages; p++) {
        ReadLine(ctx, page, Button(OptionsRow(own, g_buttons + p), Label(app::PanelPageTitle(p)), PageName(p)));
    }
    g_adding = false;
    ctx.eax = result;
}

// the page manager's page by name, thiscall(manager, name, x, y, width, height, ?, keep) ret 0x1C: ours are made as
// a menu page, never kept, so a visit reads them anew
X86_EXTERN_FUNC(__imp__sub_000291E0);
X86_FUNC(sub_000291E0) {
    std::string name = ReadName(R32(ctx.esp + 4));
    std::vector<std::string> lines;
    if (!Script(name, lines)) {
        __imp__sub_000291E0(ctx, base);
        return;
    }
    g_loading = name;
    W32(ctx.esp + 4, kMenuKindName);
    W32(ctx.esp + 0x1C, 0);
    __imp__sub_000291E0(ctx, base);
    g_loading.clear();
}

// a page reads its .swui script, thiscall(page, name) ret 4: the page being made as ours reads ours
X86_EXTERN_FUNC(__imp__sub_0001F140);
X86_FUNC(sub_0001F140) {
    g_arrowless.clear();
    if (g_loading.empty()) {
        __imp__sub_0001F140(ctx, base);
        return;
    }
    uint32_t page = ctx.ecx;
    std::string name = g_loading;
    g_loading.clear();
    std::vector<std::string> lines;
    Script(name, lines);
    std::memcpy(HostPtr(page + kPageName), name.c_str(), name.size() + 1);
    W32(page + kPageLines, 0);
    for (const std::string& line : lines) {
        W32(page + kPageLines, R32(page + kPageLines) + 1);
        if (!(ReadLine(ctx, page, line) & 0xFF)) break;
    }
    ctx.ecx = page;
    CallGuest(ctx, R32(R32(page) + kLoaded), {});
    g_scroll = 0;
    ShowValues(page);
    ctx.eax = 1;
    ctx.esp += 8;  // ret 4
}

// a menu page's input, thiscall(page): on a section page Left and Right step the focused row, A rebinds a key
X86_EXTERN_FUNC(__imp__sub_00030AF0);
X86_FUNC(sub_00030AF0) {
    uint32_t page = ctx.ecx;
    int panel_page = 0;
    size_t section = 0;
    if (!SectionPage(ReadName(page + kPageName), panel_page, section)) {
        g_arrowless.clear();
        __imp__sub_00030AF0(ctx, base);
        return;
    }
    std::vector<uint32_t> combos = Combos(page);
    int before = FocusedCombo(page, combos);
    __imp__sub_00030AF0(ctx, base);
    UpdateScroll(ctx, page, combos, before, app::PanelRows(panel_page, section).size());
    int focused = FocusedCombo(page, combos);
    if (focused >= 0) {
        size_t r = g_scroll + size_t(focused);
        uint32_t pressed = R32(page + kPressed);
        int step = ((pressed & kRight) ? 1 : 0) - ((pressed & kLeft) ? 1 : 0);
        if (step) app::StepPanelRow(panel_page, section, r, step);
        if (pressed & kConfirm) app::PressPanelRow(panel_page, section, r, 0);
        if (pressed & kButtonX) app::PressPanelRow(panel_page, section, r, 1);
    }
    ShowValues(page);
}

// a combo's draw, thiscall(combo): the key bind rows' arrows unseen, from the page's first frame
X86_EXTERN_FUNC(__imp__sub_000237A0);
X86_FUNC(sub_000237A0) {
    if (std::find(g_arrowless.begin(), g_arrowless.end(), ctx.ecx) != g_arrowless.end()) {
        G<float>(ctx.ecx + kLeftArrowAlpha) = 0.0f;
        G<float>(ctx.ecx + kRightArrowAlpha) = 0.0f;
    }
    __imp__sub_000237A0(ctx, base);
}
