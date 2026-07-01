#pragma once

#include <string>
#include <string_view>

#include "Move.h"

// Long Algebraic Notation (LAN) — the single canonical, fully-disambiguated move
// notation used across the agent boundary, move history, and UI. Unlike SAN
// (handled by `AlgebraicMove`) it always carries the full source square, so a
// move can be applied without any source-resolution / disambiguation step.
//
// Grammar:
//   - Piece prefix K/Q/R/B/N; pawns have no prefix.
//   - Full origin + destination, '-' for quiet moves (e2-e4, Ng1-f3).
//   - 'x' for captures (e4xd5, Nd4xc6).
//   - Promotion '=Q' (e7-e8=Q, e7xd8=Q).
//   - Castling O-O / O-O-O.
//   - Check '+', checkmate '#'.
//
// NOTE: distinct from the internal `LongAlgebraicMove` (Move.h), which is really
// UCI coordinate notation (e2e4, g1f3). LAN parses straight into a
// `LongAlgebraicMove` for the board's native apply path, `Board::Move(LongAlgebraicMove)`.

namespace chess::notation {

// The structural result of parsing a LAN string: the coordinate move the board
// applies, plus the notational *claims* the string made (piece, capture, check,
// checkmate) so callers can enforce them against the board's computed truth.
struct ParsedLongAlgebraicNotation {
    LongAlgebraicMove move;            // coords for the board (castling => king two-square move)
    PieceType piece = Pawn;            // claimed moving piece (King for castling)
    bool is_capture = false;           // 'x' used (vs '-')
    bool claims_check = false;         // trailing '+'
    bool claims_checkmate = false;     // trailing '#'
    bool is_castle = false;
};

// Parses a LAN string. `side_to_move` is required to resolve castling (O-O/O-O-O
// carry no squares) into the king's coordinate move. Throws
// MalformedLongAlgebraicNotationException on any unparseable input.
ParsedLongAlgebraicNotation ParseLongAlgebraicNotation(std::string_view notation, Colour side_to_move);

// Produces canonical LAN for a board-produced move. `m` is expected to come from
// `Board::Move(LongAlgebraicMove)`, whose returned AlgebraicMove carries the full
// source square in `Specifier` (low 6 bits) plus the true capture/check/mate flags.
std::string FormatLongAlgebraicNotation(const AlgebraicMove& m);

} // namespace chess::notation