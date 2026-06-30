#pragma once

#include <memory>
#include <optional>
#include <cstdint>

#include <gaunt/core/data/GauntContext.hpp>
#include <gaunt/generated/chess_coliseum.gaunt.hpp>

#include "Chess/Move.h"

namespace chess::telemetry {

namespace ggcc = gaunt::generated::chess_coliseum;

// Per-player telemetry context within a game
struct GauntPlayerContext {
    std::optional<gaunt::core::GauntContext<ggcc::context::Player>> player_context;
    std::optional<gaunt::core::GauntContext<ggcc::context::Move>> current_move_context;
    std::uint32_t move_counter{0};
};

// Per-game telemetry context
struct GauntGameContext {
    std::optional<gaunt::core::GauntContext<ggcc::context::Game>> game_context;
    GauntPlayerContext white_player;
    GauntPlayerContext black_player;
};

// Color mapping helper: convert chess Colour enum to Gaunt Color attribute
inline ggcc::attribute::Color ToGauntColor(Colour chess_color) {
    return (chess_color == White)
        ? ggcc::attribute::Color::White
        : ggcc::attribute::Color::Black;
}

} // namespace chess::telemetry