#pragma once
#include "BaseBubble.h"
#include "Utility/AgentTrajectory.h"
#include <optional>

class MoveBubble : public BaseBubble {
public:

    explicit MoveBubble(
        const std::string& message,
        const MoveVerificationState state = MoveVerificationState::Unverified,
        std::optional<std::string> errorMsg = std::nullopt
    ) : BaseBubble(message), m_State(state), m_ErrorMsg(std::move(errorMsg)) {}

    void Render(const ImVec4& border_color, const ImVec4& background_color, const ImVec4& text_color, ImFont* header_font, std::shared_ptr<SDL_Texture> icon = nullptr) override {
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

        if (header_font) {
            ImGui::PushFont(header_font);
        }
        ImGui::Text("move");
        if (icon) {
            ImGui::SameLine();
            const float icon_size = ImGui::GetFontSize();
            ImGui::Image(
                static_cast<ImTextureID>(reinterpret_cast<intptr_t>(icon.get())),
                ImVec2(icon_size, icon_size),
                ImVec2(0, 0), ImVec2(1, 1),
                text_color,
                ImVec4(0, 0, 0, 0)
            );
        }
        if (header_font) {
            ImGui::PopFont();
        }

        std::string display_message = m_Message;
        if (m_State == MoveVerificationState::Error && m_ErrorMsg) {
            display_message += "\nError: " + *m_ErrorMsg;
        }
        ImGui::TextWrapped("%s", display_message.c_str());
        
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
    std::optional<std::string> m_ErrorMsg;
};
