#pragma once
#include "BaseBubble.h"

class QuipBubble : public BaseBubble {
public:
    QuipBubble(const std::string& message) : BaseBubble(message) {}

    void Render() override {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        // Subtle purple/pink for witty remarks
        RenderBubble(ImGui::GetColorU32(ImVec4(0.3f, 0.2f, 0.35f, 1.0f)));
    }
};
