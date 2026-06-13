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
    static constexpr const char* kTypeTag = "setup";
    std::string color;
};

inline void to_json(nlohmann::json& j, const SetupRequest& m) {
    j = nlohmann::json{{"type", SetupRequest::kTypeTag}, {"color", m.color}};
}

struct SetupResponse {
    static constexpr const char* kTypeTag = "setup_ack";
    std::string model;
    std::string color;
};

inline void from_json(const nlohmann::json& j, SetupResponse& m) {
    m.model = j.value("model", "Unknown Model");
    m.color = j.value("color", "unknown");
}

} // namespace chess::agent::message
