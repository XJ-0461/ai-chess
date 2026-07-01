#pragma once

#include <exception>
#include <string>
#include <utility>

// Codified agent/move error states. Each error has a stable CODE (for logging
// and programmatic handling) plus a short human-readable description. The UI
// shows them as 'CODE - description'; a single move may accumulate several.

namespace chess::game::error {

inline constexpr auto kErrorMissingMakeMove                = "ERROR_MISSING_MAKE_MOVE";
inline constexpr auto kErrorIllegalMove                    = "ERROR_ILLEGAL_MOVE";
inline constexpr auto kErrorAmbiguousMove                  = "ERROR_AMBIGUOUS_MOVE";
inline constexpr auto kErrorInvalidNotation                = "ERROR_INVALID_NOTATION";
inline constexpr auto kErrorMalformedLongAlgebraicNotation = "ERROR_MALFORMED_LONG_ALGEBRAIC_NOTATION";
inline constexpr auto kErrorMissingCheckSymbol             = "ERROR_MISSING_CHECK_SYMBOL";
inline constexpr auto kErrorMissingCheckmateSymbol         = "ERROR_MISSING_CHECKMATE_SYMBOL";
inline constexpr auto kErrorInvalidCheckClaim              = "ERROR_INVALID_CHECK_CLAIM";
inline constexpr auto kErrorInvalidCheckmateClaim          = "ERROR_INVALID_CHECKMATE_CLAIM";
inline constexpr auto kErrorInvalidCaptureNotation         = "ERROR_INVALID_CAPTURE_NOTATION";
inline constexpr auto kErrorMissingCaptureSymbol           = "ERROR_MISSING_CAPTURE_SYMBOL";
inline constexpr auto kErrorPieceMismatch                  = "ERROR_PIECE_MISMATCH";
inline constexpr auto kErrorMoveNotApplied                 = "ERROR_MOVE_NOT_APPLIED";
inline constexpr auto kErrorAgentTimeout                   = "ERROR_AGENT_TIMEOUT";
inline constexpr auto kErrorAgentFailure                   = "ERROR_AGENT_FAILURE";

// A single codified error.
struct MoveError {
    std::string code;
    std::string description;

    [[nodiscard]] std::string Format() const { return code + " - " + description; }
};

// Base exception carrying a MoveError; validation / agent code throws a derived
// type and handlers catch MoveErrorException to read the codified `.error`.
struct MoveErrorException : std::exception {
    MoveError error;
    explicit MoveErrorException(MoveError e) : error(std::move(e)) {}
    [[nodiscard]] const char* what() const noexcept override { return error.description.c_str(); }
};

struct MissingMakeMove : MoveErrorException {
    MissingMakeMove()
        : MoveErrorException({kErrorMissingMakeMove,
            "The model ended its turn without submitting a move."}) {}
};

struct IllegalMove : MoveErrorException {
    explicit IllegalMove(const std::string& move_input)
        : MoveErrorException({kErrorIllegalMove,
            "No piece exists that satisfies move input " + move_input + "."}) {}
};

struct AmbiguousMove : MoveErrorException {
    explicit AmbiguousMove(const std::string& move_input)
        : MoveErrorException({kErrorAmbiguousMove,
            "Move input " + move_input + " is ambiguous; more than one piece can make this move."}) {}
};

struct InvalidNotation : MoveErrorException {
    explicit InvalidNotation(const std::string& move_input)
        : MoveErrorException({kErrorInvalidNotation,
            move_input + " is not valid algebraic notation."}) {}
};

// The move string could not be parsed as Long Algebraic Notation at all (e.g.
// SAN was submitted, the separator/squares are malformed). Purely notational.
struct MalformedLongAlgebraicNotation : MoveErrorException {
    explicit MalformedLongAlgebraicNotation(const std::string& move_input)
        : MoveErrorException({kErrorMalformedLongAlgebraicNotation,
            move_input + " is not valid long algebraic notation."}) {}
};

// The piece letter in the notation disagrees with the piece actually standing on
// the source square (e.g. "Ng1-f3" when g1 holds a bishop).
struct PieceMismatch : MoveErrorException {
    explicit PieceMismatch(const std::string& move_input)
        : MoveErrorException({kErrorPieceMismatch,
            "Move " + move_input + " names a piece that is not on the source square."}) {}
};

// Strict LAN suffix/separator enforcement: the move's claimed +/#/x must match the
// board's computed flags, so every accepted move string is canonical.
struct MissingCheckSymbol : MoveErrorException {
    MissingCheckSymbol()
        : MoveErrorException({kErrorMissingCheckSymbol,
            "Move gives check but omits the '+' suffix."}) {}
};

struct MissingCheckmateSymbol : MoveErrorException {
    MissingCheckmateSymbol()
        : MoveErrorException({kErrorMissingCheckmateSymbol,
            "Move delivers checkmate but omits the '#' suffix."}) {}
};

struct InvalidCheckClaim : MoveErrorException {
    explicit InvalidCheckClaim(const std::string& move_input)
        : MoveErrorException({kErrorInvalidCheckClaim,
            "Move " + move_input + " claims check (+) but does not put the opponent in check."}) {}
};

struct InvalidCheckmateClaim : MoveErrorException {
    explicit InvalidCheckmateClaim(const std::string& move_input)
        : MoveErrorException({kErrorInvalidCheckmateClaim,
            "Move " + move_input + " claims checkmate (#) but does not deliver checkmate."}) {}
};

struct InvalidCaptureNotation : MoveErrorException {
    explicit InvalidCaptureNotation(const std::string& move_input)
        : MoveErrorException({kErrorInvalidCaptureNotation,
            "Move " + move_input + " uses capture notation 'x' but there is no piece to capture."}) {}
};

struct MissingCaptureSymbol : MoveErrorException {
    explicit MissingCaptureSymbol(const std::string& move_input)
        : MoveErrorException({kErrorMissingCaptureSymbol,
            "Move " + move_input + " captures a piece but uses '-' instead of 'x'."}) {}
};

// Classifies an engine IllegalMoveException into the right codified error,
// producing a crisp, move-input-aware message (the engine already distinguishes
// the no-piece case from the multiple-piece / ambiguous case).
[[nodiscard]] inline MoveError ClassifyIllegalMove(const std::string& engine_message, const std::string& move_input) {
    if (engine_message.find("than one piece") != std::string::npos) {
        return AmbiguousMove{move_input}.error;
    }
    return IllegalMove{move_input}.error;
}

// Maps a raw, free-form agent error string to a codified MoveError so the wild
// variety of agent-side messages still surfaces with a stable code.
[[nodiscard]] inline MoveError CodifyAgentError(const std::string& raw) {
    if (raw.find("did not submit") != std::string::npos ||
        raw.find("make_move") != std::string::npos ||
        raw.find("without submitting") != std::string::npos) {
        return {kErrorMissingMakeMove,
            "The model ended its turn without submitting a move."};
    }
    if (raw.find("timed out") != std::string::npos || raw.find("timeout") != std::string::npos) {
        return {kErrorAgentTimeout, raw};
    }
    return {kErrorAgentFailure, raw};
}

} // namespace chess::game::error
