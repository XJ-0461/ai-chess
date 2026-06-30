#pragma once

#include <memory>

#include "Chess/Board.h"
#include "Game/GameMoveLog.hpp"
#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Board/BoardAtlas.hpp"
#include "Graphics/Theme/AgentSidebarColorPalette.hpp"

class CentralChessboardPanel {
public:
    CentralChessboardPanel() = default;
    ~CentralChessboardPanel() = default;

    void SetPieceAtlases(std::shared_ptr<PaletteSwappedPieceAtlas> white, std::shared_ptr<PaletteSwappedPieceAtlas> black) {
        m_WhitePieces = white;
        m_BlackPieces = black;
    }

    void SetBoardAtlas(std::shared_ptr<PaletteSwappedBoardAtlas> board) {
        m_BoardAtlas = board;
    }

    void SetGameState(std::shared_ptr<Board> board, std::shared_ptr<std::mutex> mutex) {
        m_Board = board;
        m_BoardMutex = mutex;
    }

    void SetInteractionState(Square* selectedPiece, BitBoard* legalMoves, bool* isHoldingPiece) {
        m_SelectedPiece = selectedPiece;
        m_LegalMoves = legalMoves;
        m_IsHoldingPiece = isHoldingPiece;
    }

    // Move-history bar: a per-view toggle (right-click context menu), the shared
    // move log, and the styling used so it matches the rest of the view.
    void SetMoveHistoryState(bool* showMoveHistory) { m_ShowMoveHistory = showMoveHistory; }
    void SetMoveLog(std::shared_ptr<chess::game::GameMoveLog> moveLog) { m_MoveLog = std::move(moveLog); }
    void SetHeaderFont(struct ImFont* font) { m_HeaderFont = font; }
    void SetMovePalette(const AgentSidebarColorPalette& palette) { m_MovePalette = palette; }

    // Window control: allows right-click menu to close the parent window (useful
    // when running without a title bar).
    void SetWindowOpenState(bool* windowOpen) { m_WindowOpen = windowOpen; }

    // Triggers move history bar to scroll to the rightmost position
    void ScrollMoveHistoryToEnd() { m_MoveHistoryScrollToEnd = true; }

    void Render();

private:
    void RenderMoveHistoryBar(float height);

private:
    std::shared_ptr<PaletteSwappedPieceAtlas> m_WhitePieces;
    std::shared_ptr<PaletteSwappedPieceAtlas> m_BlackPieces;
    std::shared_ptr<PaletteSwappedBoardAtlas> m_BoardAtlas;

    std::shared_ptr<Board> m_Board;
    std::shared_ptr<std::mutex> m_BoardMutex;

    // Interaction state (pointers to Application's members)
    Square* m_SelectedPiece = nullptr;
    BitBoard* m_LegalMoves = nullptr;
    bool* m_IsHoldingPiece = nullptr;

    // Move-history bar
    bool* m_ShowMoveHistory = nullptr; // owned by the GameView
    std::shared_ptr<chess::game::GameMoveLog> m_MoveLog;
    struct ImFont* m_HeaderFont = nullptr;
    AgentSidebarColorPalette m_MovePalette;

    // Auto-scroll state for move history bar
    bool m_MoveHistoryAutoScroll = true;
    bool m_MoveHistoryScrollToEnd = false;

    // Window control
    bool* m_WindowOpen = nullptr; // owned by the GameView/ThreeColumnGameWindow
};
