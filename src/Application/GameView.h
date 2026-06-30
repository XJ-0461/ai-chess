#pragma once

#include <memory>
#include <string>

#include "Chess/BitBoard.h"
#include "Chess/Move.h"
#include "Application/AgentChat/AgentSidebar.h"
#include "Application/CentralChessboardPanel.h"
#include "Application/Command/Command.hpp"
#include "Application/ThreeColumnGameWindow.hpp"
#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Pieces/PieceBorderAtlas.hpp"
#include "Graphics/Board/BoardAtlas.hpp"
#include "Game/GameContext.hpp"

namespace chess::application {

// Appearance preset for a spawned spectator view.
enum class GameViewTheme {
    Default,       // wood board (themeable per provider later)
    HighContrast,  // standard high-contrast board + standard piece set
};

// A spectator view of a single game. Owns its own GL texture atlases, board
// panel and agent sidebars, all hydrated from a shared GameContext. Multiple
// independent views of the same game can coexist, each its own window.
class GameView {
public:
    GameView(
        std::shared_ptr<chess::game::GameContext> context,
        GameViewTheme theme,
        struct ImFont* header_font,
        std::string window_title
    );

    GameView(const GameView&) = delete;
    GameView& operator=(const GameView&) = delete;

    void Render();

    [[nodiscard]] bool IsOpen() const { return open_; }
    [[nodiscard]] const std::string& GameId() const { return context_->id; }
    [[nodiscard]] const std::string& WindowTitle() const { return layout_.window_title; }

    void SetMoveHistoryBarEnabled(bool enabled) { show_move_history_ = enabled; }

    // Sets initial window configuration to be applied on first render.
    void SetInitialWindowConfig(std::shared_ptr<command::WindowConfiguration> config);

private:
    std::shared_ptr<chess::game::GameContext> context_;

    // Per-view, themed textures (owned here, created on the GL/UI thread).
    std::shared_ptr<PaletteSwappedPieceAtlas> white_pieces_;
    std::shared_ptr<PaletteSwappedPieceAtlas> black_pieces_;
    std::shared_ptr<PaletteSwappedPieceBorderAtlas> outline_pieces_; // 1px border behind captured pieces
    std::shared_ptr<PaletteSwappedBoardAtlas> board_atlas_;

    AgentSidebar white_sidebar_;
    AgentSidebar black_sidebar_;
    CentralChessboardPanel central_panel_;

    // Per-view interaction state (presentation only - not part of game state).
    Square selected_piece_{INVALID_SQUARE};
    BitBoard legal_moves_{0};
    bool is_holding_piece_{false};
    bool show_move_history_{false}; // toggled via the central panel's right-click menu

    bool open_{true};

    ThreeColumnGameWindow<AgentSidebar, CentralChessboardPanel, AgentSidebar> layout_;
};

} // namespace chess::application
