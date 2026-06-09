#pragma once

#include <imgui.h>
#include <string>
#include <vector>

class BaseBubble {
public:

    explicit BaseBubble(const std::string& message) : m_Message(message) {}
    virtual ~BaseBubble() = default;

    virtual void Render() = 0;

protected:
    /**
     * Helper to render content within a styled bubble.
     * Uses ChannelsSplit to draw the background rectangle behind the content
     * AFTER the content size has been determined.
     */
    void RenderBubble(ImU32 bgColor, const std::string& label = "") {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Split draw list into 2 channels: 0 for background, 1 for text
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1); // Start with foreground

        ImGui::BeginGroup();
        if (!label.empty()) {
            ImGui::TextDisabled("%s", label.c_str());
        }
        ImGui::TextWrapped("%s", m_Message.c_str());
        ImGui::EndGroup();

        // Get the bounding box of the group we just drew
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();

        // Apply padding
        min.x -= 10.0f;
        min.y -= 8.0f;
        max.x += 10.0f;
        max.y += 8.0f;

        // Switch to background channel and draw the rect
        drawList->ChannelsSetCurrent(0);
        drawList->AddRectFilled(min, max, bgColor, 10.0f);

        // Merge channels back
        drawList->ChannelsMerge();
        
        // Add some spacing after the bubble
        ImGui::Spacing();
        ImGui::Spacing();
    }

    std::string m_Message;
};
