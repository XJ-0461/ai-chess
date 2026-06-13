#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

struct StartMoveRequest {
    static constexpr const char* kTypeTag = "start_move";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
};

inline void to_json(nlohmann::json& j, const StartMoveRequest& m) {
    j = nlohmann::json{
        {"type", StartMoveRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips}
    };
}

struct MoveResponse {
    static constexpr const char* kTypeTag = "move_decision";
    std::string algebraic_move_string;
};

inline void from_json(const nlohmann::json& j, MoveResponse& m) {
    m.algebraic_move_string = j.value("algebraic_move_string", "");
}

struct ReasoningSnapshot {
    static constexpr const char* kTypeTag = "reasoning";
    std::string id;
    std::string message;
};

inline void from_json(const nlohmann::json& j, ReasoningSnapshot& m) {
    m.id = j.value("id", "");
    m.message = j.value("message", "");
}

struct ResponseSnapshot {
    static constexpr const char* kTypeTag = "response";
    std::string id;
    std::string message;
};

inline void from_json(const nlohmann::json& j, ResponseSnapshot& m) {
    m.id = j.value("id", "");
    m.message = j.value("message", "");
}

struct QuipResponse {
    static constexpr const char* kTypeTag = "quip";
    std::string id;
    std::string message;
};

inline void from_json(const nlohmann::json& j, QuipResponse& m) {
    m.id = j.value("id", "");
    m.message = j.value("message", "");
}

} // namespace chess::agent::message
