#pragma once

#include <string>
#include <vector>
#include <mutex>

enum class AgentState {
    Disconnected,
    Idle,
    Thinking,
    Error
};

struct ChatEvent {
    std::string id;
    std::string type;    // "reasoning", "response", "error", "info"
    std::string message;
};

struct AgentTrajectory {
    AgentState state = AgentState::Disconnected;
    std::string modelName = "Unknown Model";
    std::vector<ChatEvent> events;
    std::mutex mtx;

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
};
