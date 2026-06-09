#pragma once
#include "BaseBubble.h"

class InfoBubble : public BaseBubble {
public:
    InfoBubble(const std::string& message, bool isError = false) 
        : BaseBubble(message), m_IsError(isError) {}

    void Render() override {
        ImGui::PushStyleColor(ImGuiCol_Text, m_IsError ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextWrapped("[%s] %s", m_IsError ? "ERROR" : "INFO", m_Message.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

private:
    bool m_IsError;
};
