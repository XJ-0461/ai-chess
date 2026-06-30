#include "GameView.h"

#include <utility>

#include "Graphics/Theme/Color/WoodAgentSidebarColorPalette.hpp"

namespace chess::application {

GameView::GameView(
    std::shared_ptr<chess::game::GameContext> context,
    const GameViewTheme theme,
    struct ImFont* header_font,
    std::string window_title
) : context_(std::move(context)),
    white_sidebar_("White Agent", "WHITE"),
    black_sidebar_("Black Agent", "BLACK"),
    layout_(std::move(window_title), &white_sidebar_, &central_panel_, &black_sidebar_) {

    // Choose palettes for this view. High contrast uses the standard board;
    // the default theme uses the wood board (and is the hook for future
    // per-provider theming). Both use the standard piece set for now.
    const BoardColorPaletteT board_palette = (theme == GameViewTheme::HighContrast)
        ? kStandardBoardColorPalette
        : kWoodBoardColorPalette;

    white_pieces_ = std::make_shared<PaletteSwappedPieceAtlas>(kBasicWhiteColorPalette);
    white_pieces_->MakeTexture();
    black_pieces_ = std::make_shared<PaletteSwappedPieceAtlas>(kBasicBlackColorPalette);
    black_pieces_->MakeTexture();
    outline_pieces_ = std::make_shared<PaletteSwappedPieceBorderAtlas>(RGBA{255, 255, 255, 255});
    outline_pieces_->MakeTexture();
    board_atlas_ = std::make_shared<PaletteSwappedBoardAtlas>(board_palette);

    const AgentSidebarColorPalette sidebar_palette = chess::style::color::kWoodAgentSidebarColorPalette;

    // Hydrate the central board panel from the shared game context.
    central_panel_.SetPieceAtlases(white_pieces_, black_pieces_);
    central_panel_.SetBoardAtlas(board_atlas_);
    central_panel_.SetGameState(context_->board, context_->board_mutex);
    central_panel_.SetInteractionState(&selected_piece_, &legal_moves_, &is_holding_piece_);
    central_panel_.SetMoveHistoryState(&show_move_history_);
    central_panel_.SetMoveLog(context_->move_log);
    central_panel_.SetHeaderFont(header_font);
    central_panel_.SetMovePalette(sidebar_palette);
    central_panel_.SetWindowOpenState(&open_);

    // Hydrate the sidebars. No live trajectory yet (the sidebar renders a
    // placeholder until an agent is attached).
    white_sidebar_.SetHeaderFont(header_font);
    white_sidebar_.SetPieceAtlas(white_pieces_);
    white_sidebar_.SetCapturedPieceAtlas(black_pieces_); // white captures black pieces
    white_sidebar_.SetOutlinePieceAtlas(outline_pieces_);
    white_sidebar_.SetColorPalette(sidebar_palette);
    white_sidebar_.SetTrajectory(context_->white_trajectory);
    white_sidebar_.SetMoveLog(context_->move_log);
    black_sidebar_.SetHeaderFont(header_font);
    black_sidebar_.SetPieceAtlas(black_pieces_);
    black_sidebar_.SetCapturedPieceAtlas(white_pieces_); // black captures white pieces
    black_sidebar_.SetOutlinePieceAtlas(outline_pieces_);
    black_sidebar_.SetColorPalette(sidebar_palette);
    black_sidebar_.SetTrajectory(context_->black_trajectory);
    black_sidebar_.SetMoveLog(context_->move_log);
}

void GameView::Render() {
    layout_.Render(&open_);
}

void GameView::SetInitialWindowConfig(std::shared_ptr<command::WindowConfiguration> config) {
    layout_.SetInitialWindowConfig(std::move(config));
}

} // namespace chess::application
