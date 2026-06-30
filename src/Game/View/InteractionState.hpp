#pragma once

#include <memory>

#include "Chess/Move.h"
#include "Chess/BitBoard.h"

namespace chess::game::view {

// Per-view interaction state for a board view (piece selection / drag). This is
// a presentation concern - it belongs to a view, NOT to the game's authoritative
// state (chess::game::execution::GameOrchestratorState), so that a single game
// can drive many independent views, each with its own interaction state.
struct InteractionState {
    std::shared_ptr<Square> selected_piece;
    std::shared_ptr<BitBoard> legal_moves;
    bool is_holding_piece{false};
};

} // namespace chess::game::view
