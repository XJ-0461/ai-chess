#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

struct Ping {
    static constexpr const char* kTypeTag = "ping";
};

inline void to_json(nlohmann::json& j, const Ping&) {
    j = nlohmann::json{{"type", Ping::kTypeTag}};
}

struct Pong {
    static constexpr const char* kTypeTag = "pong";
};

inline void from_json(const nlohmann::json&, Pong&) {}

struct SetupRequest {
    static constexpr auto kTypeTag = "setup";
    std::string color;
    std::string provider;  // e.g. "OpenRouter"
    std::string model;     // model id
    std::string api_key;   // provider credential
    bool enable_quip{false};
    bool enable_draw_offer{false};
    bool enable_resignation{false};
};

inline void to_json(nlohmann::json& j, const SetupRequest& m) {
    j = nlohmann::json{
        {"type", SetupRequest::kTypeTag},
        {"color", m.color},
        {"provider", m.provider},
        {"model", m.model},
        {"api_key", m.api_key},
        {"enable_quip", m.enable_quip},
        {"enable_draw_offer", m.enable_draw_offer},
        {"enable_resignation", m.enable_resignation}
    };
}

struct SetupResponse {
    static constexpr const char* kTypeTag = "setup_ack";
    std::string model;
    std::string color;
    std::string personality;
};

inline void from_json(const nlohmann::json& j, SetupResponse& m) {
    m.model = j.value("model", "Unknown Model");
    m.color = j.value("color", "unknown");
    m.personality = j.value("personality", "");
}

} // namespace chess::agent::message
