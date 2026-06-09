#pragma once
#include "BaseBubble.h"

class ReasoningBubble : public BaseBubble {
public:
    ReasoningBubble(const std::string& message) : BaseBubble(message) {}

    void Render() override {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        
        RenderBubble(ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 1.0f)), "[Reasoning]");
        
        ImGui::PopStyleColor();
    }
};
