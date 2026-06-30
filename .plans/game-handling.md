# Game Move Handling Refactoring Plan

## Current Problems

1. `Board::Move(AlgebraicMove)` mixes validation and application - can't validate without side effects
2. Returns `LongAlgebraicMove` which is insufficient - doesn't include check/capture/promotion info
3. Uses exceptions for control flow - forces try/catch in all callers
4. No move history - can't undo or replay
5. Check/checkmate notation claims aren't validated against reality
6. Validation logic is scattered across multiple code paths

---

## Proposed Data Structures

### Move: The canonical representation of a chess move

```cpp
// src/Chess/Move.hpp (consolidate with existing Move.h)

struct Move {
    Square from;
    Square to;
    PieceType piece;                          // The piece being moved
    bool is_capture = false;
    Piece captured_piece = Piece::None;       // What was captured (if any)
    bool is_promotion = false;
    std::optional<PieceType> promotes_to;     // What pawn promotes to
    bool is_castling = false;
    std::optional<CastleSide> castle_side;
    bool is_en_passant = false;
    bool gives_check = false;
    bool gives_checkmate = false;

    // Convert to SAN string (e.g., "Nxf7+")
    [[nodiscard]] std::string ToSAN() const;

    // Convert to long algebraic (e.g., "g1f3")
    [[nodiscard]] std::string ToLongAlgebraic() const;
};
```

### MoveError: Typed validation errors

```cpp
// src/Chess/MoveError.hpp

namespace chess::error {

struct IllegalMove {
    std::string input;
    std::string reason;  // "No piece can reach this square", "Would leave king in check", etc.
};

struct AmbiguousMove {
    std::string input;
    std::vector<Square> candidates;  // Which pieces could make this move
};

struct InvalidNotation {
    std::string input;
};

struct InvalidCaptureClaim {
    std::string input;  // Move claimed capture (x) but destination is empty
};

struct InvalidCheckClaim {
    std::string input;  // Move claimed check (+) but doesn't give check
};

struct InvalidCheckmateClaim {
    std::string input;  // Move claimed checkmate (#) but doesn't deliver mate
};

struct MissingPromotion {
    std::string input;  // Pawn reached back rank without promotion specified
};

using MoveError = std::variant<
    IllegalMove,
    AmbiguousMove,
    InvalidNotation,
    InvalidCaptureClaim,
    InvalidCheckClaim,
    InvalidCheckmateClaim,
    MissingPromotion
>;

// Convert to display string for UI
[[nodiscard]] std::string FormatError(const MoveError& error);

// Convert to agent-facing error code
[[nodiscard]] std::string ErrorCode(const MoveError& error);

} // namespace chess::error
```

### ValidationResult: What ValidateMove returns

```cpp
// src/Chess/ValidationResult.hpp

namespace chess {

struct ValidationResult {
    std::optional<Move> move;           // Populated if valid
    std::vector<error::MoveError> errors;  // Populated if invalid

    [[nodiscard]] bool ok() const { return errors.empty() && move.has_value(); }
    [[nodiscard]] explicit operator bool() const { return ok(); }
};

} // namespace chess
```

---

## Board Changes

### New Board API

```cpp
// src/Chess/Board.h

class Board {
public:
    // --- Validation (const, no side effects) ---

    // Parse and validate an algebraic move string
    [[nodiscard]] ValidationResult ValidateMove(std::string_view algebraic) const;

    // Validate an already-parsed AlgebraicMove
    [[nodiscard]] ValidationResult ValidateMove(const AlgebraicMove& m) const;

    // Check if a from->to move is legal (existing, keep it)
    [[nodiscard]] bool IsMoveLegal(Square from, Square to) const;


    // --- Application (mutates board) ---

    // Apply a validated move (asserts move.ok(), no re-validation)
    void ApplyMove(const Move& move);

    // Validate then apply - returns the resolved Move or throws
    Move MakeMove(std::string_view algebraic);
    Move MakeMove(const AlgebraicMove& m);


    // --- History ---

    // Undo the last move (restores previous board state)
    void UndoLastMove();

    // Get move history
    [[nodiscard]] const std::vector<Move>& MoveHistory() const { return move_history_; }

    // Get board state at a specific ply
    [[nodiscard]] const Board& StateAtPly(std::size_t ply) const;


    // --- State Queries ---

    [[nodiscard]] bool IsInCheck(Colour c) const;
    [[nodiscard]] bool HasLegalMoves(Colour c) const;
    [[nodiscard]] bool IsCheckmate() const;  // Current player is checkmated
    [[nodiscard]] bool IsStalemate() const;  // Current player has no moves but not in check


private:
    // Internal: apply move without validation (used by ApplyMove)
    void ApplyMoveInternal(const Move& move);

    // Internal: compute what a move actually does (check, capture, etc.)
    [[nodiscard]] Move ResolveMove(Square from, Square to, const AlgebraicMove& notation) const;

    // State snapshots for undo
    struct BoardSnapshot {
        std::array<Piece, 64> board;
        std::array<BitBoard, PieceTypeCount> piece_bitboards;
        std::array<BitBoard, ColourCount> colour_bitboards;
        std::array<BitBoard, 4> castling_paths;
        Square en_passant_square;
        Colour player_turn;
    };

    std::vector<BoardSnapshot> history_;
    std::vector<Move> move_history_;
};
```

### ValidateMove Implementation Sketch

```cpp
ValidationResult Board::ValidateMove(const AlgebraicMove& m) const {
    ValidationResult result;

    // 1. Handle castling separately
    if (m.Flags & MoveFlag::CastlingFlags) {
        return ValidateCastling(m);
    }

    // 2. Find source square - which piece(s) can make this move?
    auto [source, ambiguity] = FindSourceSquare(m);

    if (source == INVALID_SQUARE) {
        if (ambiguity == Ambiguity::Multiple) {
            result.errors.push_back(error::AmbiguousMove{m.ToString(), /* candidates */});
        } else {
            result.errors.push_back(error::IllegalMove{m.ToString(), "No piece can make this move"});
        }
        return result;
    }

    // 3. Check basic legality (doesn't leave own king in check)
    if (!IsMoveLegal(source, m.Destination)) {
        result.errors.push_back(error::IllegalMove{m.ToString(), "Move would leave king in check"});
        return result;
    }

    // 4. Validate capture claim
    if (m.Flags & MoveFlag::Capture) {
        bool is_en_passant = (m.MovingPiece == Pawn && m.Destination == m_EnPassantSquare);
        if (!is_en_passant && m_Board[m.Destination] == Piece::None) {
            result.errors.push_back(error::InvalidCaptureClaim{m.ToString()});
            return result;
        }
    }

    // 5. Check promotion requirements
    if (m.MovingPiece == Pawn && IsPromotionRank(m.Destination, m_PlayerTurn)) {
        if (!(m.Flags & MoveFlag::PromotionFlags)) {
            result.errors.push_back(error::MissingPromotion{m.ToString()});
            return result;
        }
    }

    // 6. Simulate move to validate check/checkmate claims
    Board after_move = *this;  // Copy current board state
    Move resolved = after_move.ResolveMove(source, m.Destination, m);
    after_move.ApplyMoveInternal(resolved);

    // Now check what the move ACTUALLY does
    bool actually_gives_check = after_move.IsInCheck(after_move.m_PlayerTurn);
    bool actually_gives_mate = actually_gives_check && !after_move.HasLegalMoves(after_move.m_PlayerTurn);

    // Validate checkmate claim
    if ((m.Flags & MoveFlag::Checkmate) && !actually_gives_mate) {
        result.errors.push_back(error::InvalidCheckmateClaim{m.ToString()});
        return result;
    }

    // Validate check claim (only if not claiming mate, since mate implies check)
    if ((m.Flags & MoveFlag::Check) && !(m.Flags & MoveFlag::Checkmate) && !actually_gives_check) {
        result.errors.push_back(error::InvalidCheckClaim{m.ToString()});
        return result;
    }

    // 7. Build the resolved Move with actual flags
    resolved.gives_check = actually_gives_check;
    resolved.gives_checkmate = actually_gives_mate;
    result.move = resolved;

    return result;
}
```

---

## Orchestrator Changes

The orchestrator currently catches exceptions. With the new API:

```cpp
void PerformPlayerMoveAction(const mhood_t<PlayerMoveAction> move) {
    auto validate_task = [/*...*/]() {
        Board snapshot;
        {
            std::lock_guard<std::mutex> lock(*board_mutex);
            snapshot = *board;  // Copy for validation
        }

        // Parse notation first
        AlgebraicMove algebraic;
        try {
            algebraic = AlgebraicMove{move->algebraic_move_string};
        } catch (const InvalidAlgebraicMoveException&) {
            std::vector<chess::game::error::MoveError> errors = {
                {kErrorInvalidNotation, "Invalid algebraic notation: " + move->algebraic_move_string}
            };
            so_5::send<PlayerMoveActionValidationFailure>(/*...*/, errors);
            return;
        }

        // Validate
        auto result = snapshot.ValidateMove(algebraic);

        if (result.ok()) {
            so_5::send<PlayerMoveActionValidationSuccess>(/*...*/, *result.move);
        } else {
            // Convert chess::error::MoveError to chess::game::error::MoveError
            auto game_errors = ConvertErrors(result.errors);
            so_5::send<PlayerMoveActionValidationFailure>(/*...*/, game_errors);
        }
    };
}
```

---

## Migration Steps

### Step 1: Add new types (non-breaking)
- Create `src/Chess/MoveError.hpp`
- Create `src/Chess/ValidationResult.hpp`
- Extend Move struct in `src/Chess/Move.h`

### Step 2: Add ValidateMove (non-breaking)
- Add `ValidateMove()` to Board as new method
- Add helper methods: `FindSourceSquare()`, `ResolveMove()`, etc.
- Existing `Move()` unchanged

### Step 3: Add move history (non-breaking)
- Add `BoardSnapshot` and `history_` vector
- Save snapshot before each move in `Move()`
- Implement `UndoLastMove()`

### Step 4: Update orchestrator
- Switch from exception-catching to `ValidateMove()` result checking
- Update error conversion

### Step 5: Deprecate old API
- Mark `Move(AlgebraicMove)` as deprecated
- Add `MakeMove()` as replacement
- Migrate callers

### Step 6: Clean up
- Remove redundant exception types
- Consolidate error handling

---

## Files

| File | Status | Purpose |
|------|--------|---------|
| `src/Chess/Move.h` | MODIFY | Extend Move struct with full move info |
| `src/Chess/MoveError.hpp` | NEW | Typed validation errors |
| `src/Chess/ValidationResult.hpp` | NEW | ValidateMove return type |
| `src/Chess/Board.h` | MODIFY | Add ValidateMove, history, helpers |
| `src/Chess/Board.cpp` | MODIFY | Implement new methods |
| `src/Game/Execution/GameOrchestrator.hpp` | MODIFY | Use new validation API |
| `src/Game/Error/AgentMoveError.hpp` | MODIFY | Add conversion from chess::error |

---

## Questions to Resolve

1. Should `ValidateMove` also check for draw conditions (threefold repetition, 50-move rule)?
2. How should history snapshots handle memory for long games? (Consider limiting depth or using incremental encoding)
3. Should `Move` include timestamps or move numbers for logging?
