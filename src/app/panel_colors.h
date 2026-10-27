// The F10 panel's colours, the front end's sepia and dark red
#pragma once
#include <utility>

#include <imgui.h>

namespace app {

inline constexpr ImVec4 kPanelBg{0.07f, 0.05f, 0.035f, 0.94f};
inline constexpr ImVec4 kBorder{0.72f, 0.60f, 0.42f, 0.85f};
inline constexpr ImVec4 kText{0.93f, 0.88f, 0.76f, 1.0f};
inline constexpr ImVec4 kHighlight{0.40f, 0.10f, 0.07f, 1.0f};
inline constexpr ImVec4 kHeading{0.80f, 0.66f, 0.42f, 1.0f};
inline constexpr ImVec4 kTab{0.14f, 0.10f, 0.07f, 1.0f};
inline constexpr ImVec4 kTabHovered{0.22f, 0.15f, 0.10f, 1.0f};
inline constexpr ImVec4 kButton{0.18f, 0.12f, 0.08f, 1.0f};
// lighter than the highlight, so it shows on a selected row
inline constexpr ImVec4 kButtonHovered{0.55f, 0.16f, 0.10f, 1.0f};
inline constexpr ImVec4 kLine{0.80f, 0.66f, 0.42f, 0.35f};
inline constexpr ImVec4 kDim{0.62f, 0.56f, 0.46f, 1.0f};  // the footer hint

inline constexpr std::pair<ImGuiCol, ImVec4> kPanelColors[] = {
    {ImGuiCol_WindowBg, kPanelBg}, {ImGuiCol_Border, kBorder}, {ImGuiCol_Text, kText},
    {ImGuiCol_Header, kHighlight}, {ImGuiCol_HeaderHovered, kHighlight}, {ImGuiCol_HeaderActive, kHighlight},
    {ImGuiCol_Button, kButton}, {ImGuiCol_ButtonHovered, kButtonHovered}, {ImGuiCol_ButtonActive, kButtonHovered},
    {ImGuiCol_FrameBg, kTab}, {ImGuiCol_FrameBgHovered, kTabHovered}, {ImGuiCol_FrameBgActive, kHighlight},
    {ImGuiCol_ScrollbarGrab, kHighlight}, {ImGuiCol_ScrollbarGrabHovered, kButtonHovered},
    {ImGuiCol_ScrollbarGrabActive, kHeading}, {ImGuiCol_Separator, kLine}, {ImGuiCol_NavHighlight, kHeading},
    {ImGuiCol_TextSelectedBg, kButtonHovered},
};

}  // namespace app
