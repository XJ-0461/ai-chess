#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

struct GameHistory {
    static constexpr const char* kTypeTag = "game_history";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::string personality;
};

inline void to_json(nlohmann::json& j, const GameHistory& m) {
    j = nlohmann::json{
        {"type", GameHistory::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"personality", m.personality}
    };
}

struct GameEnd {
    static constexpr const char* kTypeTag = "end_game";
    std::string winner;
    std::string cause;
};

inline void to_json(nlohmann::json& j, const GameEnd& m) {
    j = nlohmann::json{
        {"type", GameEnd::kTypeTag},
        {"winner", m.winner},
        {"cause", m.cause}
    };
}

struct ErrorRecoveryRequest {
    static constexpr const char* kTypeTag = "error_recovery";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::vector<std::string> errors;
    std::string personality;
};

inline void to_json(nlohmann::json& j, const ErrorRecoveryRequest& m) {
    j = nlohmann::json{
        {"type", ErrorRecoveryRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"errors", m.errors},
        {"personality", m.personality}
    };
}

struct RetrospectiveRequest {
    static constexpr const char* kTypeTag = "retrospective_request";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::string winner;
    std::string cause;
    std::string personality;
};

inline void to_json(nlohmann::json& j, const RetrospectiveRequest& m) {
    j = nlohmann::json{
        {"type", RetrospectiveRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"winner", m.winner},
        {"cause", m.cause},
        {"personality", m.personality}
    };
}

struct ErrorResponse {
    static constexpr const char* kTypeTag = "ERROR";
    std::string message;
};

inline void from_json(const nlohmann::json& j, ErrorResponse& m) {
    m.message = j.value("message", "Unknown error");
}

} // namespace chess::agent::message
