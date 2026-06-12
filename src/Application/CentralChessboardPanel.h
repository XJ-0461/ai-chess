#pragma once

#include <memory>

#include "Chess/Board.h"
#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Board/BoardAtlas.hpp"

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

    void Render();

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

};
