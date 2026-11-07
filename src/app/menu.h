// the settings panel on F10
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace app {

bool MenuOpen();
void ToggleMenu();
bool Capturing();  // a key bind waits for a key

void DrawUi(double fps);  // GameInfo::ui
// a key message for the panel: its key, Escape while it is open, the texture replacement key; true takes it
bool MenuMessage(uint32_t msg, uintptr_t wp, intptr_t lp);

// the panel's pages and their section headings, as the game's options menu lists them
constexpr int kPanelPages = 3;
const char* PanelPageTitle(int page);
std::vector<std::string> PanelSections(int page);

// a section's rows as the panel shows them
struct PanelRow {
    std::string label;
    std::string value;
    bool bind = false;  // a key bind, its value its keys
    bool pad = false;   // a controller button bind, no alternate
};
std::vector<PanelRow> PanelRows(int page, size_t section);
void StepPanelRow(int page, size_t section, size_t row, int step);  // Left / Right
// a key bind waits for its new key, slot 0 the key, 1 the alternate
void PressPanelRow(int page, size_t section, size_t row, int slot);
// the LevelFlow entry clicked on the DEBUG page, once; 0 when none
uint32_t TakeLevelPick();
// the DEBUG page's invincibility, the game's own Invulnerable cheat
bool Invincible();

}  // namespace app
