#pragma once
#include "BaseBubble.h"
#include "Utility/AgentTrajectory.h"
#include <optional>

class MoveBubble : public BaseBubble {
public:
    MoveBubble(const std::string& message, MoveVerificationState state = MoveVerificationState::Unverified, std::optional<std::string> errorMsg = std::nullopt) 
        : BaseBubble(message), m_State(state), m_ErrorMsg(errorMsg) {}

    void Render() override {
        float padding_x = 12.0f;
        float padding_y = 10.0f;
        float margin_y = 6.0f;

        // Reserve space for margin and top padding
        ImGui::Dummy(ImVec2(0.0f, margin_y + padding_y));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImGui::BeginGroup();

        // Render verification dot
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float dotSize = ImGui::GetFontSize() * 0.5f;
        ImGui::Dummy(ImVec2(dotSize + 4.0f, dotSize));
        ImGui::SameLine();

        ImVec4 dotColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        switch (m_State) {
            case MoveVerificationState::Unverified: dotColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
            case MoveVerificationState::Verified:   dotColor = ImVec4(0.0f, 0.85f, 0.2f, 1.0f); break;
            case MoveVerificationState::Error:      dotColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
        }
        
        drawList->AddCircleFilled(
            ImVec2(pos.x + dotSize * 0.5f, pos.y + ImGui::GetTextLineHeight() * 0.4f),
            dotSize * 0.5f,
            ImGui::ColorConvertFloat4ToU32(dotColor)
        );

        ImGui::TextDisabled("Move Decision:");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); 
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("%s", m_Message.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        if (m_State == MoveVerificationState::Error && m_ErrorMsg) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", m_ErrorMsg->c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.x -= padding_x; min.y -= padding_y;
        max.x += padding_x; max.y += padding_y;

        drawList->ChannelsSetCurrent(0);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.1f, 0.35f, 0.15f, 1.0f)), 8.0f);
        drawList->ChannelsMerge();
        
        // Advance cursor past the bottom padding
        ImGui::Dummy(ImVec2(0.0f, padding_y));
    }
private:
    MoveVerificationState m_State;
    std::optional<std::string> m_ErrorMsg;
};
