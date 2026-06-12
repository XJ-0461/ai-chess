#include "AgentSidebar.h"
#include <imgui.h>
#include "ChatBubble/ReasoningBubble.h"
#include "ChatBubble/ResponseBubble.h"
#include "ChatBubble/QuipBubble.h"
#include "ChatBubble/MoveBubble.h"
#include "ChatBubble/InfoBubble.h"

AgentSidebar::AgentSidebar(const std::string& title, const std::string& colorName)
    : m_Title(title), m_ColorName(colorName) {
}

void AgentSidebar::ClearChat() {
    if (m_Trajectory) {
        std::lock_guard<std::mutex> lock(m_Trajectory->mtx);
        m_Trajectory->events.clear();
        m_Trajectory->state = AgentState::Idle;
    }
}

void AgentSidebar::Render() {
    // Render as a child window to integrate into the Layout's table cells
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    
    // Use the title for the child ID to keep state separate if multiple sidebars exist
    if (ImGui::BeginChild(m_Title.c_str(), ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {

        if (!m_Trajectory) {
            ImGui::Text("Agent not initialized.");
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        // --- Header Redesign ---
        ImGui::Spacing();
        ImGui::Indent(8.0f);
        
        float availWidth = ImGui::GetContentRegionAvail().x - 8.0f; // Account for indent
        float picSize = std::clamp(availWidth * 0.25f, 48.0f, 80.0f);
        float padding = 8.0f;

        ImGui::BeginGroup();
        {
            // Left: Profile Pic Placeholder
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(picSize, picSize));
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(
                cursor, 
                ImVec2(cursor.x + picSize, cursor.y + picSize), 
                ImGui::GetColorU32(ImGuiCol_Text, 0.5f), 
                4.0f
            );
            
            // "PFP" label inside (temporary)
            const char* label = "PFP";
            ImVec2 labelSize = ImGui::CalcTextSize(label);
            drawList->AddText(
                ImVec2(cursor.x + (picSize - labelSize.x) * 0.5f, cursor.y + (picSize - labelSize.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text, 0.3f),
                label
            );

            ImGui::SameLine(0, padding);

            // Right: Future Area
            if (ImGui::BeginChild("FutureArea", ImVec2(0, picSize), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                // ImGui::TextDisabled("Future...");
                ImGui::EndChild();
            }
        }
        ImGui::EndGroup();

        ImGui::Spacing();

        // Model Name with Custom Font
        if (m_HeaderFont) ImGui::PushFont(m_HeaderFont);
        ImGui::Text("%s", m_Trajectory->modelName.c_str());
        if (m_HeaderFont) ImGui::PopFont();

        ImGui::Unindent(8.0f);
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Spacing();

        // --- Chat Logs Child Area ---
        if (ImGui::BeginChild((m_Title + "##ChatHistory").c_str(), ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            {
                std::lock_guard<std::mutex> lock(m_Trajectory->mtx);
                for (const auto& ev : m_Trajectory->events) {
                    if (ev.type == "reasoning") {
                        ReasoningBubble(ev.message).Render();
                    } else if (ev.type == "response") {
                        ResponseBubble(ev.message).Render();
                    } else if (ev.type == "quip") {
                        QuipBubble(ev.message).Render();
                    } else if (ev.type == "move") {
                        MoveBubble(ev.message, ev.verificationState, ev.errorMessage).Render();
                    } else if (ev.type == "error") {
                        InfoBubble(ev.message, true).Render();
                    } else {
                        InfoBubble(ev.message).Render();
                    }
                }
            }

            if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
                ImGui::SetScrollHereY(1.0f);
                m_ScrollToBottom = false;
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
