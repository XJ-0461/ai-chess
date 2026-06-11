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

#include "Utility/AgentTrajectory.h"
#include <memory>

class AgentSidebar {
public:
    AgentSidebar(const std::string& title, const std::string& colorName);
    ~AgentSidebar() = default;

    void Render();

    void SetTrajectory(std::shared_ptr<AgentTrajectory> trajectory) { m_Trajectory = trajectory; }
    void ClearChat();

private:
    std::string m_Title;        // Window title (e.g. "White Agent" or "Black Agent")
    std::string m_ColorName;    // "WHITE" or "BLACK"
    std::shared_ptr<AgentTrajectory> m_Trajectory;

    bool m_AutoScroll = true;   // Auto-scroll enabled by default
    bool m_ScrollToBottom = false;
};
