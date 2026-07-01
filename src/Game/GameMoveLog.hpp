#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Chess/Move.h"
#include "Game/Error/AgentMoveError.hpp"

namespace chess::game {

// The outcome of a single move attempt by a player.
struct MoveOutcome {
    std::string id;        // correlation id, shared with the trajectory entry
    Colour color{White};
    std::string long_algebraic_notation;       // the submitted move string (Long Algebraic Notation)
    bool accepted{false};  // true if it was legal and applied to the board
    std::vector<error::MoveError> errors; // codified reasons it was rejected
};

// Thread-safe, ordered record of every move attempt in a game. Written by the
// GameOrchestrator; read by the agent sidebar view-model (move bubble
// hydration, by id) and the move-history bar (accepted moves, in order).
class GameMoveLog {
public:
    void Append(MoveOutcome outcome) {
        const std::lock_guard<std::mutex> lock(mtx_);
        outcomes_.push_back(std::move(outcome));
    }

    // Look up a specific attempt by its correlation id.
    [[nodiscard]] std::optional<MoveOutcome> Find(const std::string& id) const {
        const std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& outcome : outcomes_) {
            if (outcome.id == id) {
                return outcome;
            }
        }
        return std::nullopt;
    }

    // Accepted moves in play order — the move-history bar source.
    [[nodiscard]]
    std::vector<std::string> AcceptedMoves() const {
        const std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> result;
        for (const auto& outcome : outcomes_) {
            if (outcome.accepted) {
                result.push_back(outcome.long_algebraic_notation);
            }
        }
        return result;
    }

    // All move outcomes in attempt order — for audio event detection.
    [[nodiscard]]
    std::vector<MoveOutcome> AllOutcomes() const {
        const std::lock_guard<std::mutex> lock(mtx_);
        return outcomes_;
    }

private:
    mutable std::mutex mtx_;
    std::vector<MoveOutcome> outcomes_;
};

} // namespace chess::game
