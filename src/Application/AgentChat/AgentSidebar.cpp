#include "AgentSidebar.h"

#include <imgui.h>
#include <ctime>
#include <iomanip>
#include <sstream>

AgentSidebar::AgentSidebar(const std::string& title, const std::string& colorName)
    : m_Title(title), m_ColorName(colorName), m_Status("Disconnected")
{
    // Initialize default names based on color
    if (colorName == "WHITE") {
        m_AgentName = "DeepChess White";
        m_ModelName = "meta-llama/llama-3.1-405b";
    } else {
        m_AgentName = "DeepChess Black";
        m_ModelName = "meta-llama/llama-3.1-70b";
    }
}

void AgentSidebar::SetAgentInfo(const std::string& name, const std::string& model) {
    m_AgentName = name;
    m_ModelName = model;
}

void AgentSidebar::SetStatus(const std::string& status) {
    m_Status = status;
}

void AgentSidebar::StartNewTurn(int moveIndex) {
    AgentTurn turn;
    turn.moveIndex = moveIndex;
    turn.modelName = m_ModelName;
    turn.isThinking = true;
    turn.showReasoning = true; // uncollapsed by default
    turn.timestamp = std::chrono::system_clock::now();

    m_Turns.push_back(turn);
    m_ScrollToBottom = true;
}

void AgentSidebar::AddReasoningDelta(const std::string& delta) {
    if (m_Turns.empty()) {
        StartNewTurn(1);
    }
    m_Turns.back().reasoning += delta;
    if (m_AutoScroll) {
        m_ScrollToBottom = true;
    }
}

void AgentSidebar::AddResponseDelta(const std::string& delta) {
    if (m_Turns.empty()) {
        StartNewTurn(1);
    }
    m_Turns.back().response += delta;
    if (m_AutoScroll) {
        m_ScrollToBottom = true;
    }
}

void AgentSidebar::SetMoveDecision(const std::string& move) {
    if (m_Turns.empty()) {
        StartNewTurn(1);
    }
    m_Turns.back().moveDecision = move;
    m_Turns.back().isThinking = false;
    m_ScrollToBottom = true;
}

void AgentSidebar::ClearChat() {
    m_Turns.clear();
    m_Status = "Ready";
}

void AgentSidebar::Render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::Begin(m_Title.c_str());

    // --- Header ---
    ImGui::PushID("HeaderInfo");
    
    // Status Dot drawing
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float dotSize = ImGui::GetFontSize() * 0.5f;
    ImGui::Dummy(ImVec2(dotSize + 4.0f, dotSize));
    ImGui::SameLine();
    
    ImVec4 dotColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Default disconnected Gray
    if (m_Status == "Thinking...") {
        dotColor = ImVec4(1.0f, 0.75f, 0.0f, 1.0f); // Amber / Yellow
    } else if (m_Status == "Ready" || m_Status == "Connected") {
        dotColor = ImVec4(0.0f, 0.85f, 0.2f, 1.0f); // Green
    }
    
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(pos.x + dotSize * 0.5f, pos.y + ImGui::GetTextLineHeight() * 0.4f),
        dotSize * 0.5f,
        ImGui::ColorConvertFloat4ToU32(dotColor)
    );

    ImGui::Text("%s", m_AgentName.c_str());
    ImGui::PopID();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("Model: %s", m_ModelName.c_str());
    ImGui::Text("Status: %s", m_Status.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // --- Chat Logs Child Area ---
    ImGui::BeginChild("##ChatHistory", ImVec2(0, -45), false, ImGuiChildFlags_AlwaysUseWindowPadding);

    for (size_t i = 0; i < m_Turns.size(); ++i) {
        RenderTurn(m_Turns[i], static_cast<int>(i));
        if (i < m_Turns.size() - 1) {
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    if (m_ScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_ScrollToBottom = false;
    }

    ImGui::EndChild();

    // --- Footer Controls ---
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
    ImGui::SameLine();
    
    if (ImGui::Button("Clear")) {
        ClearChat();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void AgentSidebar::RenderTurn(const AgentTurn& turn, int index) {
    ImGui::PushID(index);

    // Format Timestamp
    std::time_t time = std::chrono::system_clock::to_time_t(turn.timestamp);
    std::tm timeInfo;
#if defined(_MSC_VER)
    localtime_s(&timeInfo, &time);
#else
    localtime_r(&time, &timeInfo);
#endif
    std::stringstream timeStr;
    timeStr << std::put_time(&timeInfo, "%H:%M:%S");

    // Display Move Header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::Text("Move %d  [%s]", turn.moveIndex, timeStr.str().c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Display Thoughts (Collapsible)
    std::string thoughtLabel = (turn.showReasoning ? "v Thoughts" : "> Thoughts");
    if (turn.isThinking && turn.moveDecision.empty() && turn.response.empty()) {
        thoughtLabel += " (Thinking...)";
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Invisible button background
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.7f, 1.0f, 1.0f)); // Custom blue accent link style
    if (ImGui::Button(thoughtLabel.c_str())) {
        const_cast<AgentTurn&>(turn).showReasoning = !turn.showReasoning;
    }
    ImGui::PopStyleColor(3);

    if (turn.showReasoning) {
        ImGui::Spacing();
        
        // Draw the reasoning section with a left vertical border line
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        float lineHeight = 0.0f;
        
        ImGui::BeginGroup();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f); // Indent the text
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.62f, 0.65f, 1.0f));
        ImGui::PushTextWrapPos(0.0f);
        if (turn.reasoning.empty()) {
            ImGui::TextDisabled("...");
        } else {
            ImGui::TextUnformatted(turn.reasoning.c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        
        ImGui::EndGroup();

        // Calculate height of text block and draw left vertical line
        ImVec2 endPos = ImGui::GetCursorScreenPos();
        lineHeight = endPos.y - startPos.y;
        
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(startPos.x + 4.0f, startPos.y),
            ImVec2(startPos.x + 4.0f, startPos.y + lineHeight - 4.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.32f, 1.0f)),
            2.0f
        );
        ImGui::Spacing();
    }

    // Display Response Commentary
    if (!turn.response.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(turn.response.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }

    // Display Move Decision
    if (!turn.moveDecision.empty()) {
        ImGui::Spacing();
        ImGui::Text("Play Decision: ");
        ImGui::SameLine();
        
        // Render move decision as a highlighted tag
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.25f, 1.0f)); // Nice green button
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::Button(turn.moveDecision.c_str(), ImVec2(50, 0));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}
