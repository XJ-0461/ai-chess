#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Everything a player needs to act on its turn. Built by the orchestrator from
// the authoritative game state and handed to the player so the request payload
// it sends to its agent carries the up-to-date position, prior errors, etc.
struct TurnContext {
    std::vector<std::string> game_history{};   // moves played so far, in order (SAN)
    std::vector<std::string> opponent_quips{};
    std::vector<std::string> own_quip_history{}; // this player's quips so far (avoid repetition)
    std::vector<std::string> errors{};          // why the previous attempt failed (recovery)
    std::uint32_t turn_number{1};               // full-move number
    std::string winner{};                       // retrospective only
    std::string cause{};                        // retrospective only
};

struct IPlayer {

    virtual ~IPlayer() = default;

    virtual void BeginTurn(const TurnContext& context) = 0;
    virtual void BeginHandleDrawOffer(const TurnContext& context) = 0;
    virtual void BeginHandleOpponentDeclinedDrawOffer(const TurnContext& context) = 0;
    virtual void BeginErrorRecoveryTurn(const TurnContext& context) = 0;
    virtual void BeginRetrospectiveTurn(const TurnContext& context) = 0;

};
