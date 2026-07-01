#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <cstdint>

#include <gaunt/core/data/GauntContext.hpp>
#include <gaunt/generated/chess_coliseum.gaunt.hpp>

#include "Chess/Move.h"
#include "Game/Error/AgentMoveError.hpp"

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

// Collapses a move's accumulated codified errors into the single MoveInputError
// the SubmitInvalidMove telemetry event carries. Errors are ranked by a fixed
// severity priority (most fundamental notation/legality faults first, decoration
// claim mismatches last) and the highest-priority code present wins. Codes with
// no MoveInputError counterpart (agent/system failures such as kErrorAgentFailure
// or kErrorMoveNotApplied) are ignored; if none of the errors map, returns
// nullopt and the caller should skip SubmitInvalidMove entirely.
inline std::optional<ggcc::attribute::MoveInputError>
SelectMoveInputError(const std::vector<game::error::MoveError>& errors) {
    namespace err = chess::game::error;
    using MIE = ggcc::attribute::MoveInputError;

    // Highest severity first.
    static constexpr std::pair<const char*, MIE> kPriority[] = {
        {err::kErrorMalformedLongAlgebraicNotation, MIE::MalformedLongAlgebraicNotation},
        {err::kErrorInvalidNotation,                MIE::InvalidLongAlgebraicNotation},
        {err::kErrorIllegalMove,                    MIE::IllegalMove},
        {err::kErrorAmbiguousMove,                  MIE::AmbiguousMove},
        {err::kErrorPieceMismatch,                  MIE::PieceMismatch},
        {err::kErrorInvalidCaptureNotation,         MIE::InvalidCaptureNotation},
        {err::kErrorMissingCaptureSymbol,           MIE::MissingCaptureSymbol},
        {err::kErrorInvalidCheckmateClaim,          MIE::InvalidCheckmateClaim},
        {err::kErrorMissingCheckmateSymbol,         MIE::MissingCheckmateSymbol},
        {err::kErrorInvalidCheckClaim,              MIE::InvalidCheckClaim},
        {err::kErrorMissingCheckSymbol,             MIE::MissingCheckSymbol},
    };

    for (const auto& [code, variant] : kPriority) {
        for (const auto& move_error : errors) {
            if (move_error.code == code) {
                return variant;
            }
        }
    }
    return std::nullopt;
}

} // namespace chess::telemetry