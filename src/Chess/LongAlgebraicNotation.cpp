#include "LongAlgebraicNotation.hpp"

#include <cctype>

#include "ChessException.h"

namespace chess::notation {

ParsedLongAlgebraicNotation ParseLongAlgebraicNotation(std::string_view notation, Colour side_to_move) {
    const std::string original{notation};
    std::string_view remaining = notation;

    ParsedLongAlgebraicNotation parsed{};

    // Trailing check / checkmate suffix.
    if (!remaining.empty() && remaining.back() == '#') {
        parsed.claims_checkmate = true;
        remaining.remove_suffix(1);
    } else if (!remaining.empty() && remaining.back() == '+') {
        parsed.claims_check = true;
        remaining.remove_suffix(1);
    }

    // Castling — no squares in the string, so resolve from the side to move.
    if (remaining == "O-O" || remaining == "O-O-O") {
        const Square rank_offset = static_cast<Square>(side_to_move * 0b00111000);
        const Square king_source = static_cast<Square>(E1 ^ rank_offset);
        const Square king_destination = (remaining == "O-O")
            ? static_cast<Square>(G1 ^ rank_offset)
            : static_cast<Square>(C1 ^ rank_offset);

        parsed.is_castle = true;
        parsed.piece = King;
        parsed.move = LongAlgebraicMove{king_source, king_destination};
        return parsed;
    }

    // Trailing promotion "=X".
    if (remaining.size() >= 2 && remaining[remaining.size() - 2] == '=') {
        switch (remaining.back()) {
            case 'N': {
                parsed.move.Promotion = Knight;
                break;
            }
            case 'B': {
                parsed.move.Promotion = Bishop;
                break;
            }
            case 'R': {
                parsed.move.Promotion = Rook;
                break;
            }
            case 'Q': {
                parsed.move.Promotion = Queen;
                break;
            }
            default: {
                throw MalformedLongAlgebraicNotationException(original);
            }
        }
        remaining.remove_suffix(2);
    }

    // Optional leading piece letter (pawns omit it).
    std::size_t prefix_length = 0;
    if (!remaining.empty() && std::isupper(static_cast<unsigned char>(remaining.front()))) {
        switch (remaining.front()) {
            case 'K': {
                parsed.piece = King;
                break;
            }
            case 'Q': {
                parsed.piece = Queen;
                break;
            }
            case 'R': {
                parsed.piece = Rook;
                break;
            }
            case 'B': {
                parsed.piece = Bishop;
                break;
            }
            case 'N': {
                parsed.piece = Knight;
                break;
            }
            default: {
                throw MalformedLongAlgebraicNotationException(original);
            }
        }
        prefix_length = 1;
    } else {
        parsed.piece = Pawn;
    }

    // The remainder must be exactly: source(2) separator(1) destination(2).
    if (remaining.size() - prefix_length != 5) {
        throw MalformedLongAlgebraicNotationException(original);
    }

    const char source_file = remaining[prefix_length];
    const char source_rank = remaining[prefix_length + 1];
    const char separator = remaining[prefix_length + 2];
    const char destination_file = remaining[prefix_length + 3];
    const char destination_rank = remaining[prefix_length + 4];

    const auto is_valid_file = [](char file) {
        return file >= 'a' && file <= 'h';
    };
    const auto is_valid_rank = [](char rank) {
        return rank >= '1' && rank <= '8';
    };

    if (!is_valid_file(source_file) || !is_valid_rank(source_rank) ||
        !is_valid_file(destination_file) || !is_valid_rank(destination_rank)) {
        throw MalformedLongAlgebraicNotationException(original);
    }

    if (separator == 'x') {
        parsed.is_capture = true;
    } else if (separator == '-') {
        parsed.is_capture = false;
    } else {
        throw MalformedLongAlgebraicNotationException(original);
    }

    parsed.move.SourceSquare = ToSquare(source_file, source_rank);
    parsed.move.DestinationSquare = ToSquare(destination_file, destination_rank);

    return parsed;
}

std::string FormatLongAlgebraicNotation(const AlgebraicMove& move) {
    const auto with_check_suffix = [&move](std::string notation) {
        if (move.Flags & MoveFlag::Checkmate) {
            notation += '#';
        } else if (move.Flags & MoveFlag::Check) {
            notation += '+';
        }
        return notation;
    };

    if (move.Flags & MoveFlag::CastleKingSide) {
        return with_check_suffix("O-O");
    }
    if (move.Flags & MoveFlag::CastleQueenSide) {
        return with_check_suffix("O-O-O");
    }

    std::string notation;

    if (move.MovingPiece != Pawn) {
        notation += PieceTypeToChar(move.MovingPiece);
    }

    // Board::Move(LongAlgebraicMove) stows the full source square in Specifier.
    const Square source_square = move.Specifier & RemoveSpecifierFlag;
    notation += static_cast<char>('a' + FileOf(source_square));
    notation += static_cast<char>('1' + RankOf(source_square));

    notation += (move.Flags & MoveFlag::Capture) ? 'x' : '-';

    notation += static_cast<char>('a' + FileOf(move.Destination));
    notation += static_cast<char>('1' + RankOf(move.Destination));

    if (move.Flags & MoveFlag::PromotionFlags) {
        static const char s_PromotionChars[] = " NBRQ";  // indexed by PromotionFlags (1..4)
        notation += '=';
        notation += s_PromotionChars[move.Flags & MoveFlag::PromotionFlags];
    }

    return with_check_suffix(std::move(notation));
}

} // namespace chess::notation