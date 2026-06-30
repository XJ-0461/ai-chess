#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "Chess/Move.h"

namespace chess::game::execution {

// The decisive outcome of a match. NoContest is distinct from a Draw: it means
// the game ended on a technical glitch / exception (e.g. an agent repeatedly
// failing to produce a legal move and forfeiting), so the result is void rather
// than a real half-point draw.
enum class MatchOutcome : std::uint8_t {
    InProgress,
    WhiteWin,
    BlackWin,
    Draw,
    NoContest
};

struct MatchResult {
    MatchOutcome outcome{MatchOutcome::InProgress};
    // How the game ended: "checkmate", "stalemate", "draw_agreement",
    // "resignation", "forfeit_no_move", "exception", ...
    std::string cause{};
};

// Score text shown in a given player's sidebar. White's view of a white win is
// "1-0"; black's view of the same game is "0-1". Draws are symmetric.
[[nodiscard]] inline std::string ScoreString(const MatchOutcome outcome, const Colour view) {
    switch (outcome) {
        case MatchOutcome::WhiteWin: return view == White ? "1-0" : "0-1";
        case MatchOutcome::BlackWin: return view == White ? "0-1" : "1-0";
        case MatchOutcome::Draw:     return "0.5-0.5";
        case MatchOutcome::NoContest: return "NO_CONTEST";
        case MatchOutcome::InProgress: default: return {};
    }
}

[[nodiscard]] inline std::string_view OutcomeToString(const MatchOutcome outcome) {
    switch (outcome) {
        case MatchOutcome::WhiteWin:  return "white_win";
        case MatchOutcome::BlackWin:  return "black_win";
        case MatchOutcome::Draw:      return "draw";
        case MatchOutcome::NoContest: return "no_contest";
        case MatchOutcome::InProgress: default: return "in_progress";
    }
}

// "white" | "black" | null  — only win outcomes have a winner.
[[nodiscard]] inline nlohmann::json WinnerToJson(const MatchOutcome outcome) {
    switch (outcome) {
        case MatchOutcome::WhiteWin: return "white";
        case MatchOutcome::BlackWin: return "black";
        default: return nullptr;
    }
}

inline void to_json(nlohmann::json& j, const MatchResult& result) {
    j = nlohmann::json{
        {"outcome", OutcomeToString(result.outcome)},
        {"winner", WinnerToJson(result.outcome)},
        {"cause", result.cause}
    };
}

} // namespace chess::game::execution
