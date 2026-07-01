#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

struct StartMoveRequest {
    static constexpr const char* kTypeTag = "start_move";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::string personality;
    uint32_t turn_number;
};

inline void to_json(nlohmann::json& j, const StartMoveRequest& m) {
    j = nlohmann::json{
        {"type", StartMoveRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"personality", m.personality},
        {"turn_number", m.turn_number}
    };
}

struct MoveResponse {
    static constexpr const char* kTypeTag = "move_decision";
    std::string long_algebraic_move_string;
};

inline void from_json(const nlohmann::json& j, MoveResponse& m) {
    m.long_algebraic_move_string = j.value("long_algebraic_move_string", "");
}

struct ResignResponse {
    static constexpr const char* kTypeTag = "resign";
};

inline void from_json(const nlohmann::json&, ResignResponse&) {}

struct OfferDrawResponse {
    static constexpr const char* kTypeTag = "offer_draw";
};

inline void from_json(const nlohmann::json&, OfferDrawResponse&) {}

struct DrawOfferRequest {
    static constexpr const char* kTypeTag = "draw_offer";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::string personality;
};

inline void to_json(nlohmann::json& j, const DrawOfferRequest& m) {
    j = nlohmann::json{
        {"type", DrawOfferRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"personality", m.personality}
    };
}

struct DrawDecisionResponse {
    static constexpr const char* kTypeTag = "draw_decision";
    bool accept;
};

inline void from_json(const nlohmann::json& j, DrawDecisionResponse& m) {
    m.accept = j.value("accept", false);
}

struct DrawOfferDeclinedRequest {
    static constexpr const char* kTypeTag = "draw_declined";
    std::vector<std::string> game_history;
    std::vector<std::string> opponent_quips;
    std::vector<std::string> own_quip_history;
    std::string personality;
};

inline void to_json(nlohmann::json& j, const DrawOfferDeclinedRequest& m) {
    j = nlohmann::json{
        {"type", DrawOfferDeclinedRequest::kTypeTag},
        {"game_history", m.game_history},
        {"opponent_quips", m.opponent_quips},
        {"own_quip_history", m.own_quip_history},
        {"personality", m.personality}
    };
}

struct ModelReasoningSnapshot {
    static constexpr auto kTypeTag = "model_reasoning";
    std::string id;
    std::string message;
};

inline void from_json(const nlohmann::json& j, ModelReasoningSnapshot& m) {
    m.id = j.value("id", "");
    m.message = j.value("message", "");
}

struct ModelResponseSnapshot {
    static constexpr auto kTypeTag = "model_response";
    std::string id;
    std::string message;
};

inline void from_json(const nlohmann::json& j, ModelResponseSnapshot& m) {
    m.id = j.value("id", "");
    m.message = j.value("message", "");
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
