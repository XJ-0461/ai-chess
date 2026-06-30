#pragma once

#include <vector>

#include "BaseBubble.h"
#include "Utility/AgentTrajectory.h"
#include "Game/Error/AgentMoveError.hpp"

class MoveBubble : public BaseBubble {
public:

    explicit MoveBubble(
        const std::string& message,
        const MoveVerificationState state = MoveVerificationState::Unverified,
        std::vector<chess::game::error::MoveError> errors = {},
        std::size_t moveCount = 0
    ) : BaseBubble(message, moveCount), m_State(state), m_Errors(std::move(errors)) {}

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

        RenderHeader("Move", text_color, header_font, icon);

        // The move itself (e.g. "Ra6").
        ImGui::TextWrapped("%s", m_Message.c_str());

        // On error, a horizontal rule separates the move from the codified
        // error(s); each error shows its CODE in the header font followed by a
        // concise description.
        if (m_State == MoveVerificationState::Error && !m_Errors.empty()) {
            ImGui::NewLine();
            for (const auto& error : m_Errors) {
                if (header_font) ImGui::PushFont(header_font);
                ImGui::TextWrapped("%s", error.code.c_str());
                if (header_font) ImGui::PopFont();
                ImGui::TextWrapped("%s", error.description.c_str());
            }
        }

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
private:
    MoveVerificationState m_State;
    std::vector<chess::game::error::MoveError> m_Errors;
};
