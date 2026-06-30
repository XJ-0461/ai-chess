#pragma once
#include "BaseBubble.h"

class ReasoningBubble : public BaseBubble {
public:
    ReasoningBubble(const std::string& message, std::size_t moveCount = 0) : BaseBubble(message, moveCount) {}

    void Render(const ImVec4& border_color, const ImVec4& background_color, const ImVec4& text_color, ImFont* header_font, ImTextureID icon = 0) override {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

        const ImU32 bg_u32 = ImGui::ColorConvertFloat4ToU32(background_color);
        const ImU32 border_u32 = ImGui::ColorConvertFloat4ToU32(border_color);

        constexpr float padding_x = 10.0f;
        constexpr float padding_y = 8.0f;
        constexpr float margin_y = 6.0f;

        ImGui::Dummy(ImVec2(0.0f, margin_y));

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(1);

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);

        RenderHeader("Reasoning", text_color, header_font, icon);

        ImGui::TextWrapped("%s", m_Message.c_str());
        
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.x -= padding_x; min.y -= padding_y;
        max.x += padding_x; max.y += padding_y;

        draw_list->ChannelsSetCurrent(0);
        draw_list->AddRectFilled(min, max, bg_u32, 0.0f);
        draw_list->AddRect(min, max, border_u32, 0.0f, ImDrawFlags_None, 1.0f);

        draw_list->ChannelsMerge();
        ImGui::Dummy(ImVec2(0.0f, padding_y));
    }
};
