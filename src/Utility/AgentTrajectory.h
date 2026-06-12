#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>

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

struct ChatEvent {
    std::string id;
    std::string type;    // "reasoning", "response", "error", "info", "move"
    std::string message;
    MoveVerificationState verificationState = MoveVerificationState::Unverified;
    std::optional<std::string> errorMessage = std::nullopt;
};

struct AgentTrajectory {
    AgentState state = AgentState::Disconnected;
    std::string modelName = "Unknown Model";
    std::vector<ChatEvent> events;
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

    void UpdateEvent(const std::string& id, const std::string& type, const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Try to find existing event with this ID
        for (auto& ev : events) {
            if (!id.empty() && ev.id == id) {
                ev.message = message;
                return;
            }
        }

        // Not found or no ID, create new
        events.push_back({ id, type, message });
    }

    void AddEvent(const std::string& type, const std::string& message) {
        UpdateEvent("", type, message);
    }

    void SetState(AgentState newState) {
        std::lock_guard<std::mutex> lock(mtx);
        state = newState;
    }

    void SetMoveVerificationError(const std::string& moveStr, const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (it->type == "move" && it->message == moveStr) {
                it->verificationState = MoveVerificationState::Error;
                it->errorMessage = errorMsg;
                break;
            }
        }
    }

    void SetMoveVerified(const std::string& moveStr) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            if (it->type == "move" && it->message == moveStr) {
                it->verificationState = MoveVerificationState::Verified;
                break;
            }
        }
    }
};
