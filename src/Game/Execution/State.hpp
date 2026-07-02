#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <oneapi/tbb/concurrent_vector.h>

#include "Chess/Board.h"
#include "Game/Play/IPlayer.hpp"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Game/GameLifecyclePhase.hpp"
#include "Game/GameMoveLog.hpp"
#include "Game/Execution/MatchResult.hpp"
#include "Utility/AgentTrajectory.h"
#include "Application/AgentChat/Message/Usage.hpp"
#include "Telemetry/GameTelemetry.hpp"

namespace chess::game::execution {

// Factory for constructing players from the application environment.
// (Placeholder; fleshed out when player construction is moved here.)
struct PlayerFactory {};

// Ambient application services available to a running game.
struct ApplicationEnvironment {
    std::shared_ptr<PlayerFactory> player_factory{};
};

// The authoritative board for the game. This is the source of truth that any
// number of concurrently spawned views render from - it deliberately holds no
// view/interaction concerns.
struct GameState {
    std::shared_ptr<Board> board;
    std::shared_ptr<std::mutex> board_mutex; // shared with views/players for safe access
    std::vector<std::string> move_history{};  // moves played so far, in order (SAN)
    std::vector<std::string> white_quip_history{};  // all quips white has made (for self-context)
    std::vector<std::string> black_quip_history{};  // all quips black has made (for self-context)
};

// The players attached to the game.
struct PlayerState {
    std::shared_ptr<IPlayer> white{};
    std::shared_ptr<IPlayer> black{};
};

struct RetrospectiveState {
    std::size_t retrospective_action_counter{}; // divide by 2 to get the number of retrospective turns
};

// Aggregate of everything the orchestrator owns for a single game. Note there
// is intentionally no interaction/view state here: see
// chess::game::view::InteractionState for per-view interaction state.
struct GameOrchestratorState {
    ApplicationEnvironment environment{};
    GameConfiguration game_configuration{};
    PlayerState players{};
    GameState game_state{};
    RetrospectiveState retrospective_state{};

    // Optional caller-provided id; if empty the orchestrator generates one.
    std::string game_id{};

    // Optional sink the orchestrator writes its coarse lifecycle phase into on
    // every state change, so external observers can poll progress.
    std::shared_ptr<std::atomic<GameLifecyclePhase>> published_phase{};

    // Optional sink the orchestrator writes the final MatchResult into, once,
    // before it publishes the Concluded phase. External observers read it after
    // observing Concluded (the phase store provides the happens-before).
    std::shared_ptr<MatchResult> published_result{};

    // Per-player trajectories handed to the RemoteAgentPlayers so their streamed
    // events surface in spectator views. The parallel usage-history / pricing
    // slots are shared with GameContext and drive the game::estimate_cost command.
    std::shared_ptr<AgentTrajectory> white_trajectory{};
    std::shared_ptr<tbb::concurrent_vector<chess::agent::message::ModelUsage>> white_usage_history{};
    std::shared_ptr<chess::agent::message::ModelPricing> white_model_pricing{};
    std::shared_ptr<AgentTrajectory> black_trajectory{};
    std::shared_ptr<tbb::concurrent_vector<chess::agent::message::ModelUsage>> black_usage_history{};
    std::shared_ptr<chess::agent::message::ModelPricing> black_model_pricing{};

    // Shared ordered move-outcome log (success/error per attempt).
    std::shared_ptr<GameMoveLog> move_log{};

    // Gaunt telemetry context for this game
    std::shared_ptr<chess::telemetry::GauntGameContext> gaunt_context{};
};

} // namespace chess::game::execution
