#pragma once

#include <string>
#include <vector>

#include "Chess/Move.h"
#include "Game/Error/AgentMoveError.hpp"

namespace chess::game::execution {

// Results emitted by the (off-thread) move-validation task.

struct PlayerMoveActionValidationSuccess {
    Colour color;
    std::string long_algebraic_move_string{};
    std::string id{};
};

struct PlayerMoveActionValidationFailure {
    Colour color;
    std::string long_algebraic_move_string{};
    std::string id{};
    std::vector<error::MoveError> errors{};
};

} // namespace chess::game::execution
