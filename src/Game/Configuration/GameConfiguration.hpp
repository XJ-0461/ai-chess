#pragma once

#include <string>

namespace chess::game {

struct GameConfiguration {
    std::string white_endpoint{};
    std::string black_endpoint{};
    bool enable_quip{};
    bool enable_draw_offer{};
    bool enable_resignation{};
    std::size_t retrospective_turn_count{};
};

} // namespace chess::game
