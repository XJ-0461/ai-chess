#pragma once

#include <memory>

#include "Chess/Move.h"
#include "Game/Play/IPlayer.hpp"

namespace chess::game::execution {

// Orchestration lifecycle commands/signals that drive the actor's state
// machine through setup, play, conclusion and retrospective.

struct BeginSetupRequest {};
struct CheckSetup {};

struct AttachPlayer {
    Colour colour;
    std::shared_ptr<IPlayer> player;
};

struct StartGame {};
struct EndGame {};
struct TurnTransition {};

struct BeginTurn {
    Colour color;
};

struct BeginErrorRecoveryTurn {};
struct BeginRetrospectiveTurn {};

} // namespace chess::game::execution
