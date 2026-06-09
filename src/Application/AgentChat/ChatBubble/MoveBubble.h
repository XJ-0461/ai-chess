#pragma once
#include "BaseBubble.h"

class MoveBubble : public BaseBubble {
public:
    MoveBubble(const std::string& message) : BaseBubble(message) {}

    void Render() override {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::BeginGroup();
        ImGui::TextDisabled("Move Decision:");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("%s", m_Message.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
        ImGui::EndGroup();

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.x -= 12.0f; min.y -= 10.0f;
        max.x += 12.0f; max.y += 10.0f;

        drawList->ChannelsSetCurrent(0);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.1f, 0.35f, 0.15f, 1.0f)), 8.0f);
        drawList->ChannelsMerge();
        
        ImGui::Spacing();
    }
};
