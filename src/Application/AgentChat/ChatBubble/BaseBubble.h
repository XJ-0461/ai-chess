#pragma once

#include <cstddef>
#include <string>
#include <memory>

#include <imgui.h>

class BaseBubble {
public:
    explicit BaseBubble(const std::string& message, std::size_t moveCount = 0)
        : m_Message(message), m_MoveCount(moveCount) {}
    virtual ~BaseBubble() = default;

    /**
     * @brief Pure virtual render function.
     * Each derived class is fully responsible for its own layout and rendering.
     */
    virtual void Render(
        const ImVec4& border_color,
        const ImVec4& background_color,
        const ImVec4& text_color,
        ImFont* header_font,
        ImTextureID icon = 0
    ) = 0;

protected:
    std::string m_Message;
    std::size_t m_MoveCount{0};

    // Renders the bubble title (prefixed with the move number when known) plus
    // an optional icon. The icon is tinted with the text colour so it always
    // matches the header text.
    void RenderHeader(const char* label, const ImVec4& text_color, ImFont* header_font, ImTextureID icon) {
        if (header_font) ImGui::PushFont(header_font);
        if (m_MoveCount > 0) {
            ImGui::Text("%zu - %s", m_MoveCount, label);
        } else {
            ImGui::Text("%s", label);
        }
        if (icon != 0) {
            ImGui::SameLine();
            const float icon_size = ImGui::GetFontSize();
            ImGui::Image(
                icon,
                ImVec2(icon_size, icon_size),
                ImVec2(0, 0), ImVec2(1, 1),
                text_color, ImVec4(0, 0, 0, 0)
            );
        }
        if (header_font) ImGui::PopFont();
    }
};
