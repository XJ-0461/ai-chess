#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <so_5/all.hpp>

#include "Chess/Board.h"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Game/GameLifecyclePhase.hpp"
#include "Game/GameMoveLog.hpp"
#include "Game/Execution/MatchResult.hpp"
#include "Utility/AgentTrajectory.h"

namespace chess::game {

// Defined in Game/GameAudio.hpp; forward-declared here so this widely-included
// header doesn't pull in SDL_mixer. Owned via shared_ptr below.
struct GameAudio;

// Shared, observable handle to a configured/running game. The GameOrchestrator
// actor writes the board (the single source of truth); any number of spawned
// views read it. Owned via shared_ptr so it outlives both the orchestrator
// coop and the views that reference it.
struct GameContext {
    std::string id{};
    GameConfiguration configuration{};

    // Source-of-truth board, shared with the orchestrator. Guarded by
    // board_mutex because the orchestrator (environment threads) writes it while
    // views (UI thread) read it.
    std::shared_ptr<Board> board{};
    std::shared_ptr<std::mutex> board_mutex{};

    // Direct mbox of the owning orchestrator; send lifecycle commands here
    // (e.g. StartGame).
    so_5::mbox_t command_mbox{};

    // Coarse lifecycle phase, written by the orchestrator and polled by views /
    // the command runner. Shared so it outlives either side independently.
    std::shared_ptr<std::atomic<GameLifecyclePhase>> phase{};

    // Final match result, written once by the orchestrator before it publishes
    // the Concluded phase. Safe to read after observing phase >= Concluded.
    std::shared_ptr<execution::MatchResult> result{};

    // Per-player agent trajectories: written by the RemoteAgentPlayers (on their
    // request threads) and rendered live by spectator-view sidebars.
    std::shared_ptr<AgentTrajectory> white_trajectory{};
    std::shared_ptr<AgentTrajectory> black_trajectory{};

    // Independent per-game SFX/Music mixers (allocated in CreateGame; their
    // MIX devices are opened on the main thread). Volume is controlled from the
    // GameBrowser.
    std::shared_ptr<GameAudio> audio{};

    // Ordered record of every move attempt + outcome. Written by the
    // orchestrator; read by the sidebar move bubbles and the move-history bar.
    std::shared_ptr<GameMoveLog> move_log{};
};

} // namespace chess::game
