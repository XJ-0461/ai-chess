#pragma once
#include "BaseBubble.h"

class ResponseBubble : public BaseBubble {
public:
    ResponseBubble(const std::string& message) : BaseBubble(message) {}

    void Render() override {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        RenderBubble(ImGui::GetColorU32(ImVec4(0.18f, 0.22f, 0.3f, 1.0f)));
    }
};
