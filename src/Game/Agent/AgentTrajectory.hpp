#ifdef IGNORE_THIS_FILE_MIGRATING_AWAY_FROM_THIS_FILE_
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <variant>

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

} // namespace chess::agent

struct AgentTrajectory {
    AgentState state = AgentState::Disconnected;
    std::string modelName = "Unknown Model";
    std::vector<chess::agent::ChatEventVariant> events;
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

    template<typename T>
    void UpdateEvent(const std::string& id, T&& event) {
        std::lock_guard<std::mutex> lock(mtx);

        if constexpr (std::is_same_v<std::decay_t<T>, chess::agent::message::ReasoningSnapshot> ||
            std::is_same_v<std::decay_t<T>, chess::agent::message::ResponseSnapshot> ||
            std::is_same_v<std::decay_t<T>, chess::agent::message::QuipResponse>
        ) {
            for (auto& ev : events) {
                if (auto* p = std::get_if<std::decay_t<T>>(&ev)) {
                    if (!id.empty() && p->id == id) {
                        *p = std::forward<T>(event);
                        return;
                    }
                }
            }
        }

        events.emplace_back(std::forward<T>(event));
    }

    void AddEvent(chess::agent::ChatEventVariant&& event) {
        std::lock_guard<std::mutex> lock(mtx);
        events.push_back(std::move(event));
    }

    void SetState(AgentState newState) {
        std::lock_guard<std::mutex> lock(mtx);
        state = newState;
    }

    void SetMoveVerificationError(const std::string& moveStr, const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (auto* p = std::get_if<chess::agent::MoveEvent>(&(*it))) {
                if (p->response.long_algebraic_move_string == moveStr) {
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
            if (auto* p = std::get_if<chess::agent::MoveEvent>(&(*it))) {
                if (p->response.long_algebraic_move_string == moveStr) {
                    p->verificationState = MoveVerificationState::Verified;
                    break;
                }
            }
        }
    }
};

#endif