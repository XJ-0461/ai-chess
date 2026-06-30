#pragma once

#include <string>

namespace chess::game::execution {

// Outbound notification published on the orchestrator's public state-change
// mbox so external observers (views, browsers) can react to state transitions.
struct GameStateChanged {
    std::string game_id{};
    std::string state_query_name{};
};

} // namespace chess::game::execution
