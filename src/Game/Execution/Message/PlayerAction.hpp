#pragma once

#include <string>
#include <vector>

#include <so_5/all.hpp>

#include "Chess/Move.h"

namespace chess::game::execution {

// Player-originated actions. A player may queue exactly one of these per turn;
// they are accumulated during the player's turn and dispatched on PlayerYield.

struct PlayerMoveAction {
    Colour color;
    std::string long_algebraic_move_string{};
    std::string id{}; // correlation id, shared with the trajectory entry + move log
};

struct PlayerOfferDrawAction {
    Colour color;
};

struct PlayerAcceptDrawAction {
    Colour color;
};

struct PlayerDeclineDrawAction {
    Colour color;
};

struct PlayerSubmitResignationAction {
    Colour color;
};

// Free-form commentary a player may emit alongside its action.
struct PlayerQuip {
    std::string quip{};
};

// The player's agent failed to produce a valid action this turn (e.g. the model
// finished without submitting a move). Drives error recovery / forfeit.
struct PlayerError {
    Colour color;
    std::string message{};
};

// Signals that the player has finished emitting messages for this turn, so the
// orchestrator can dispatch the accumulated action.
struct PlayerYield : so_5::signal_t {};

} // namespace chess::game::execution
