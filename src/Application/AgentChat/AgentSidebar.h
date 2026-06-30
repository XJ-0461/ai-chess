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
#include "Application/AgentChat/AgentSidebarTrajectory.hpp"
#include "Game/GameMoveLog.hpp"
#include "Graphics/Theme/AgentSidebarColorPalette.hpp"
#include <memory>

class PaletteSwappedPieceAtlas;
class PaletteSwappedPieceBorderAtlas;

class AgentSidebar {
public:
    AgentSidebar(const std::string& title, const std::string& colorName);
    ~AgentSidebar() = default;

    void Render();

    void SetTrajectory(std::shared_ptr<AgentTrajectory> trajectory) { m_Trajectory = trajectory; }
    void SetHeaderFont(struct ImFont* font) { m_HeaderFont = font; }
    void SetPieceAtlas(std::shared_ptr<PaletteSwappedPieceAtlas> atlas) { m_PieceAtlas = atlas; }
    // The opponent's atlas, used to render the (opponent-coloured) pieces this
    // agent has captured in the Future area.
    void SetCapturedPieceAtlas(std::shared_ptr<PaletteSwappedPieceAtlas> atlas) { m_CapturedPieceAtlas = atlas; }
    // A 1px-dilated single-colour border atlas drawn behind each captured piece
    // (at the same size) so it stands out against the background.
    void SetOutlinePieceAtlas(std::shared_ptr<PaletteSwappedPieceBorderAtlas> atlas) { m_OutlinePieceAtlas = atlas; }
    void SetColorPalette(const AgentSidebarColorPalette& palette) { m_ColorPalette = palette; }
    // Shared move-outcome log used to hydrate move bubbles (verified/error + codes).
    void SetMoveLog(std::shared_ptr<chess::game::GameMoveLog> move_log) { m_MoveLog = std::move(move_log); }
    void ClearChat();

private:
    std::string m_Title;        // Window title (e.g. "White Agent" or "Black Agent")
    std::string m_ColorName;    // "WHITE" or "BLACK"
    std::shared_ptr<AgentTrajectory> m_Trajectory;
    struct ImFont* m_HeaderFont = nullptr;
    std::shared_ptr<PaletteSwappedPieceAtlas> m_PieceAtlas;
    std::shared_ptr<PaletteSwappedPieceAtlas> m_CapturedPieceAtlas; // opponent's atlas
    std::shared_ptr<PaletteSwappedPieceBorderAtlas> m_OutlinePieceAtlas; // 1px border
    AgentSidebarColorPalette m_ColorPalette;

    std::shared_ptr<chess::game::GameMoveLog> m_MoveLog;
    chess::application::AgentSidebarTrajectory m_SidebarTrajectory; // view-model, rebuilt each frame

    bool m_AutoScroll = true;   // Auto-scroll enabled by default
    bool m_ScrollToBottom = false;
};
