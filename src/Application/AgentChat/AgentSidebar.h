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
#include "Graphics/Theme/AgentSidebarColorPalette.hpp"
#include <memory>

class PaletteSwappedPieceAtlas;

class AgentSidebar {
public:
    AgentSidebar(const std::string& title, const std::string& colorName);
    ~AgentSidebar() = default;

    void Render();

    void SetTrajectory(std::shared_ptr<AgentTrajectory> trajectory) { m_Trajectory = trajectory; }
    void SetHeaderFont(struct ImFont* font) { m_HeaderFont = font; }
    void SetPieceAtlas(std::shared_ptr<PaletteSwappedPieceAtlas> atlas) { m_PieceAtlas = atlas; }
    void SetColorPalette(const AgentSidebarColorPalette& palette) { m_ColorPalette = palette; }
    void ClearChat();

private:
    std::string m_Title;        // Window title (e.g. "White Agent" or "Black Agent")
    std::string m_ColorName;    // "WHITE" or "BLACK"
    std::shared_ptr<AgentTrajectory> m_Trajectory;
    struct ImFont* m_HeaderFont = nullptr;
    std::shared_ptr<PaletteSwappedPieceAtlas> m_PieceAtlas;
    AgentSidebarColorPalette m_ColorPalette;

    bool m_AutoScroll = true;   // Auto-scroll enabled by default
    bool m_ScrollToBottom = false;
};
