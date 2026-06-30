#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <variant>

#include "Chess/Move.h"
#include "Utility/Random.hpp"
#include "Application/AgentChat/Message/Handshake.hpp"
#include "Application/AgentChat/Message/GameFlow.hpp"
#include "Application/AgentChat/Message/Utility.hpp"
#include "Application/AgentChat/Message/BoardState.hpp"

enum class MoveVerificationState {
    Unverified,
    Verified,
    Error
};

enum class AgentState {
    Disconnected,
    Idle,
    Thinking,
    Error
};

namespace chess::agent {

struct InfoEvent {
    std::string message;
    bool isError = false;
};

struct MoveEvent {
    message::MoveResponse response;
    MoveVerificationState verificationState = MoveVerificationState::Unverified;
    std::optional<std::string> errorMessage = std::nullopt;
};

using ChatEventVariant = std::variant<
    InfoEvent,
    message::ReasoningSnapshot,
    message::ResponseSnapshot,
    message::QuipResponse,
    MoveEvent,
    message::ErrorResponse
>;

// A stored trajectory event with a stable id (used to correlate it with the
// sidebar view-model bubble and, for moves, the GameMoveLog outcome) and the
// full-move number it belongs to (0 for pre-game / handshake events).
struct TrajectoryEntry {
    std::string id;
    std::size_t move_count{0};
    ChatEventVariant event;
};

} // namespace chess::agent

struct AgentTrajectory {
    AgentState state = AgentState::Disconnected;
    std::string modelName = "Unknown Model";
    std::vector<chess::agent::TrajectoryEntry> events;

    // The full-move number new/updated events are stamped with. Set by the
    // player as each turn begins.
    std::size_t current_move_count = 0;

    // Opponent pieces this agent has captured, in capture order, and the
    // final score text ("1-0" / "0-1" / "0.5-0.5" / "NO_CONTEST") once the game
    // concludes. Both are written by the orchestrator and read by the sidebar;
    // guarded by mtx like the rest of this struct.
    std::vector<Piece> capturedPieces;
    std::optional<std::string> resultScore;

    std::mutex mtx;

    std::function<void(const std::string&)> onModelUpdateCallback;

    void SetOnModelUpdateCallback(std::function<void(const std::string&)> cb) {
        onModelUpdateCallback = cb;
    }

    void UpdateModelName(const std::string& name) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            modelName = name;
        }
        if (onModelUpdateCallback) {
            onModelUpdateCallback(name);
        }
    }

    void SetCurrentMoveCount(std::size_t move_count) {
        std::lock_guard<std::mutex> lock(mtx);
        current_move_count = move_count;
    }

    template<typename T>
    void UpdateEvent(const std::string& id, T&& event) {
        std::lock_guard<std::mutex> lock(mtx);

        if constexpr (std::is_same_v<std::decay_t<T>, chess::agent::message::ReasoningSnapshot> ||
            std::is_same_v<std::decay_t<T>, chess::agent::message::ResponseSnapshot> ||
            std::is_same_v<std::decay_t<T>, chess::agent::message::QuipResponse>
        ) {
            for (std::size_t i = 0; i < events.size(); ++i) {
                if (auto* p = std::get_if<std::decay_t<T>>(&events[i].event)) {
                    if (!id.empty() && p->id == id) {
                        events[i].id = id;
                        events[i].event = std::forward<T>(event);
                        events[i].move_count = current_move_count;
                        // Streamed responses can arrive out of order; an update
                        // to an older message moves it to the back so the chat
                        // history reflects the most-recently-updated order.
                        if (i + 1 != events.size()) {
                            chess::agent::TrajectoryEntry moved = std::move(events[i]);
                            events.erase(events.begin() + static_cast<std::ptrdiff_t>(i));
                            events.push_back(std::move(moved));
                        }
                        return;
                    }
                }
            }
        }

        events.push_back(chess::agent::TrajectoryEntry{
            id.empty() ? chess::util::RandomAlphaString(8) : id, current_move_count, std::forward<T>(event)});
    }

    // Appends an event. `id` correlates the entry with the sidebar bubble and,
    // for moves, the GameMoveLog outcome; a random id is generated if omitted.
    void AddEvent(chess::agent::ChatEventVariant&& event, std::string id = {}) {
        std::lock_guard<std::mutex> lock(mtx);
        if (id.empty()) {
            id = chess::util::RandomAlphaString(8);
        }
        events.push_back(chess::agent::TrajectoryEntry{std::move(id), current_move_count, std::move(event)});
    }

    void SetState(AgentState newState) {
        std::lock_guard<std::mutex> lock(mtx);
        state = newState;
    }

    void SetMoveVerificationError(const std::string& moveStr, const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (auto* p = std::get_if<chess::agent::MoveEvent>(&it->event)) {
                if (p->response.algebraic_move_string == moveStr) {
                    p->verificationState = MoveVerificationState::Error;
                    p->errorMessage = errorMsg;
                    break;
                }
            }
        }
    }

    void SetMoveVerified(const std::string& moveStr) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (auto* p = std::get_if<chess::agent::MoveEvent>(&it->event)) {
                if (p->response.algebraic_move_string == moveStr) {
                    p->verificationState = MoveVerificationState::Verified;
                    break;
                }
            }
        }
    }

    void AddCapturedPiece(Piece piece) {
        std::lock_guard<std::mutex> lock(mtx);
        capturedPieces.push_back(piece);
    }

    std::vector<Piece> GetCapturedPieces() {
        std::lock_guard<std::mutex> lock(mtx);
        return capturedPieces;
    }

    void SetResultScore(std::string score) {
        std::lock_guard<std::mutex> lock(mtx);
        resultScore = std::move(score);
    }

    std::optional<std::string> GetResultScore() {
        std::lock_guard<std::mutex> lock(mtx);
        return resultScore;
    }
};
