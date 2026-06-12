#include "CentralChessboardPanel.h"
#include "Chess/Board.h"
#include <imgui.h>
#include <algorithm>

void CentralChessboardPanel::Render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginChild("CentralChessboardPanel", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
        if (!m_WhitePieces || !m_BlackPieces || !m_BoardAtlas || !m_Board || !m_BoardMutex || 
            !m_SelectedPiece || !m_LegalMoves || !m_IsHoldingPiece) {
            ImGui::Text("Resources not initialized");
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        const float board_size = std::min(canvas_size.x, canvas_size.y);
        
        // Center the board
        const ImVec2 board_pos = ImVec2(
            canvas_pos.x + (canvas_size.x - board_size) * 0.5f,
            canvas_pos.y + (canvas_size.y - board_size) * 0.5f
        );

        // Calculate playable area (128x128 in a 142x142 texture)
        const float scale = board_size / 142.0f;
        const float inset = 7.0f * scale;
        const float playable_size = 128.0f * scale;
        const float square_size = playable_size / 8.0f;

        const ImVec2 playable_pos = ImVec2(board_pos.x + inset, board_pos.y + inset);

        const auto is_within_playable_region = [playable_pos, playable_size](float x, float y) {
            return x >= playable_pos.x
                && x < playable_pos.x + playable_size
                && y >= playable_pos.y
                && y < playable_pos.y + playable_size;
        };

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // 1. Draw Board Texture
        auto boardView = m_BoardAtlas->GetView();
        draw_list->AddImage(
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(boardView.texture)),
            board_pos,
            ImVec2(board_pos.x + board_size, board_pos.y + board_size)
        );

        std::lock_guard<std::mutex> lock(*m_BoardMutex);

        // 2. Interaction Logic
        const ImVec2 mouse_pos = ImGui::GetMousePos();
        Square hovered_square = INVALID_SQUARE;
        if (is_within_playable_region(mouse_pos.x, mouse_pos.y)) {
            const std::int32_t file = static_cast<std::int32_t>((mouse_pos.x - playable_pos.x) / square_size);
            const std::int32_t rank = 7 - static_cast<std::int32_t>((mouse_pos.y - playable_pos.y) / square_size); // Flip Y for internal board representation
            hovered_square = static_cast<Square>((rank * 8) + file);
        }

        if (ImGui::IsItemHovered() || is_within_playable_region(mouse_pos.x, mouse_pos.y)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                *m_IsHoldingPiece = true;
                if (hovered_square != INVALID_SQUARE) {
                    if (*m_SelectedPiece != INVALID_SQUARE && *m_SelectedPiece != hovered_square) {
                        if (*m_LegalMoves & (1ull << hovered_square)) {
                            m_Board->Move({ *m_SelectedPiece, hovered_square });
                        }
                        *m_SelectedPiece = INVALID_SQUARE;
                        *m_LegalMoves = 0;
                    } else {
                        *m_LegalMoves = m_Board->GetPieceLegalMoves(hovered_square);
                        *m_SelectedPiece = (*m_LegalMoves == 0) ? INVALID_SQUARE : hovered_square;
                    }
                } else {
                    *m_SelectedPiece = INVALID_SQUARE;
                    *m_LegalMoves = 0;
                }
            } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (*m_IsHoldingPiece && *m_SelectedPiece != INVALID_SQUARE && hovered_square != INVALID_SQUARE) {
                    if (*m_LegalMoves & (1ull << hovered_square)) {
                        m_Board->Move({ *m_SelectedPiece, hovered_square });
                        *m_LegalMoves = 0;
                    }
                }
                *m_IsHoldingPiece = false;
                if (!ImGui::IsItemHovered()) *m_SelectedPiece = INVALID_SQUARE; // Simplified selection
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                *m_SelectedPiece = INVALID_SQUARE;
                *m_LegalMoves = 0;
                *m_IsHoldingPiece = false;
            }
        }

        // 3. Draw Highlights (Selected & Legal Moves)
        auto get_square_rect = [&](Square s) {
            const std::int32_t file = FileOf(s);
            const std::int32_t rank = RankOf(s);
            const ImVec2 p0 = ImVec2(playable_pos.x + file * square_size, playable_pos.y + (7 - rank) * square_size);
            const ImVec2 p1 = ImVec2(p0.x + square_size, p0.y + square_size);
            return std::make_pair(p0, p1);
        };

        if (*m_SelectedPiece != INVALID_SQUARE) {
            auto [p0, p1] = get_square_rect(*m_SelectedPiece);
            draw_list->AddRectFilled(p0, p1, IM_COL32(255, 255, 0, 100)); // Yellow highlight
        }

        for (BitBoard moves = *m_LegalMoves; moves; moves &= moves - 1) {
            Square s = GetSquare(moves);
            auto [p0, p1] = get_square_rect(s);
            draw_list->AddRectFilled(p0, p1, IM_COL32(255, 0, 255, 100)); // Magenta highlight
        }

        // 4. Draw Pieces
        auto get_piece_view = [&](Piece p) -> TextureView {
            if (p == None) return { nullptr, {0,0,0,0} };
            const auto& atlas = (GetColour(p) == White) ? m_WhitePieces : m_BlackPieces;
            switch (GetPieceType(p)) {
                case Pawn:   return atlas->GetPawnTexture();
                case Knight: return atlas->GetKnightTexture();
                case Bishop: return atlas->GetBishopTexture();
                case Rook:   return atlas->GetRookTexture();
                case Queen:  return atlas->GetQueenTexture();
                case King:   return atlas->GetKingTexture();
                default:     return { nullptr, {0,0,0,0} };
            }
        };

        for (int i = 0; i < 64; ++i) {
            const Square s = static_cast<Square>(i);
            Piece p = (*m_Board)[s];
            if (p != None && (!*m_IsHoldingPiece || s != *m_SelectedPiece)) {
                auto view = get_piece_view(p);
                auto [p0, p1] = get_square_rect(s);
                ImVec2 uv0 = { view.region.x / PaletteSwappedPieceAtlas::kTotalWidth, view.region.y / PaletteSwappedPieceAtlas::kPieceHeight };
                ImVec2 uv1 = { (view.region.x + view.region.w) / PaletteSwappedPieceAtlas::kTotalWidth, (view.region.y + view.region.h) / PaletteSwappedPieceAtlas::kPieceHeight };
                draw_list->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(view.texture)), p0, p1, uv0, uv1);
            }
        }

        // 5. Draw Dragged Piece
        if (*m_IsHoldingPiece && *m_SelectedPiece != INVALID_SQUARE) {
            Piece p = (*m_Board)[*m_SelectedPiece];
            if (p != None) {
                const auto view = get_piece_view(p);
                const ImVec2 p0 = ImVec2(mouse_pos.x - square_size * 0.5f, mouse_pos.y - square_size * 0.5f);
                const ImVec2 p1 = ImVec2(p0.x + square_size, p0.y + square_size);
                const ImVec2 uv0 = { view.region.x / PaletteSwappedPieceAtlas::kTotalWidth, view.region.y / PaletteSwappedPieceAtlas::kPieceHeight };
                const ImVec2 uv1 = { (view.region.x + view.region.w) / PaletteSwappedPieceAtlas::kTotalWidth, (view.region.y + view.region.h) / PaletteSwappedPieceAtlas::kPieceHeight };
                draw_list->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(view.texture)), p0, p1, uv0, uv1);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
