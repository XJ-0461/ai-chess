#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

struct GetBoardStateRequest {
    static constexpr const char* kTypeTag = "fetch_board_state";
    std::string id;
};

inline void from_json(const nlohmann::json& j, GetBoardStateRequest& m) {
    m.id = j.value("id", "");
}

struct GetBoardStateResponse {
    static constexpr const char* kTypeTag = "board_state_response";
    std::string id;
    nlohmann::json data;
};

inline void to_json(nlohmann::json& j, const GetBoardStateResponse& m) {
    j = m.data;
    j["type"] = GetBoardStateResponse::kTypeTag;
    j["id"] = m.id;
}

struct EndTurnRequest {
    static constexpr const char* kTypeTag = "end_turn";
    std::string id;
};

inline void from_json(const nlohmann::json& j, EndTurnRequest& m) {
    m.id = j.value("id", "");
}

} // namespace chess::agent::message
