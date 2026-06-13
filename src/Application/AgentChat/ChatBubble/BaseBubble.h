#pragma once

#include <string>
#include <memory>

#include <imgui.h>
#include <SDL3/SDL.h>

class BaseBubble {
public:
    explicit BaseBubble(const std::string& message) : m_Message(message) {}
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
        std::shared_ptr<SDL_Texture> icon = nullptr
    ) = 0;

protected:
    std::string m_Message;
};
