#pragma once

#include <string_view>

namespace chess::game {

// Coarse, observable lifecycle phase of a game. Published by the orchestrator
// into the shared GameContext so other threads (e.g. the UI command runner) can
// poll progress without subscribing to the actor's mbox. Ordered so callers can
// use simple comparisons (e.g. phase >= Ready).
enum class GameLifecyclePhase {
    Initial = 0,
    Configured = 1,
    Ready = 2,
    InProgress = 3,
    Concluded = 4,
    Terminating = 5,
};

// Maps a GameOrchestrator state's query_name() to a coarse lifecycle phase.
[[nodiscard]] inline GameLifecyclePhase MapPhase(const std::string_view query_name) {
    if (query_name == "initial") {
        return GameLifecyclePhase::Initial;
    }
    if (query_name == "configured") {
        return GameLifecyclePhase::Configured;
    }
    if (query_name == "ready") {
        return GameLifecyclePhase::Ready;
    }
    if (query_name == "terminating") {
        return GameLifecyclePhase::Terminating;
    }
    if (query_name == "concluded" || query_name.find("retrospective") != std::string_view::npos) {
        return GameLifecyclePhase::Concluded;
    }
    // white_turn / black_turn / white_action / black_action are in-progress substates.
    return GameLifecyclePhase::InProgress;
}

[[nodiscard]] inline const char* GameLifecyclePhaseLabel(const GameLifecyclePhase phase) {
    switch (phase) {
    case GameLifecyclePhase::Initial:     return "Initial";
    case GameLifecyclePhase::Configured:  return "Configured";
    case GameLifecyclePhase::Ready:       return "Ready";
    case GameLifecyclePhase::InProgress:  return "In Progress";
    case GameLifecyclePhase::Concluded:   return "Concluded";
    case GameLifecyclePhase::Terminating: return "Terminating";
    }
    return "Unknown";
}

} // namespace chess::game
