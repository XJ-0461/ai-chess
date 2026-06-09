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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::Begin(m_Title.c_str());

    if (!m_Trajectory) {
        ImGui::Text("Agent not initialized.");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // --- Header ---
    AgentState state;
    {
        std::lock_guard<std::mutex> lock(m_Trajectory->mtx);
        state = m_Trajectory->state;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float dotSize = ImGui::GetFontSize() * 0.5f;
    ImGui::Dummy(ImVec2(dotSize + 4.0f, dotSize));
    ImGui::SameLine();

    ImVec4 dotColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    std::string statusStr = "Disconnected";

    switch (state) {
        case AgentState::Idle:
            dotColor = ImVec4(0.0f, 0.85f, 0.2f, 1.0f);
            statusStr = "Ready";
            break;
        case AgentState::Thinking:
            dotColor = ImVec4(1.0f, 0.75f, 0.0f, 1.0f);
            statusStr = "Thinking...";
            break;
        case AgentState::Error:
            dotColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            statusStr = "Error";
            break;
        default: break;
    }

    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(pos.x + dotSize * 0.5f, pos.y + ImGui::GetTextLineHeight() * 0.4f),
        dotSize * 0.5f,
        ImGui::ColorConvertFloat4ToU32(dotColor)
    );

    ImGui::Text("%s Agent - %s", m_ColorName.c_str(), m_Trajectory->modelName.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("Status: %s", statusStr.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // --- Chat Logs Child Area ---
    ImGui::BeginChild("##ChatHistory", ImVec2(0, 0), false, ImGuiChildFlags_AlwaysUseWindowPadding);

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

    ImGui::End();
    ImGui::PopStyleVar();
}
