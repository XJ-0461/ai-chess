#pragma once

#include <string>
#include <vector>
#include <chrono>

struct AgentTurn {
    int moveIndex = 0;
    std::string modelName;
    std::string reasoning;
    std::string response;
    std::string moveDecision;
    bool isThinking = false;
    bool showReasoning = true; // Every new thought block stream starts uncollapsed (true)
    std::chrono::system_clock::time_point timestamp;
};

class AgentSidebar {
public:
    AgentSidebar(const std::string& title, const std::string& colorName);
    ~AgentSidebar() = default;

    void Render();
    
    // Setters called by the ZMQ communication backend/application logic
    void SetAgentInfo(const std::string& name, const std::string& model);
    void SetStatus(const std::string& status);
    
    void StartNewTurn(int moveIndex);
    void AddReasoningDelta(const std::string& delta);
    void AddResponseDelta(const std::string& delta);
    void SetMoveDecision(const std::string& move);
    void ClearChat();

    // Query state
    const std::string& GetAgentName() const { return m_AgentName; }
    const std::string& GetModelName() const { return m_ModelName; }
    const std::string& GetStatus() const { return m_Status; }
    const std::vector<AgentTurn>& GetTurns() const { return m_Turns; }

private:
    void RenderTurn(const AgentTurn& turn, int index);

private:
    std::string m_Title;        // Window title (e.g. "White Agent" or "Black Agent")
    std::string m_ColorName;    // "WHITE" or "BLACK"
    std::string m_AgentName;    // e.g. "Deep Chess"
    std::string m_ModelName;    // e.g. "meta-llama/llama-3.1-70b-instruct"
    std::string m_Status;       // e.g. "Disconnected", "Ready", "Thinking..."
    std::vector<AgentTurn> m_Turns;

    bool m_AutoScroll = true;   // Auto-scroll enabled by default
    bool m_ScrollToBottom = false;
};
