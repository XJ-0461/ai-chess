#pragma once

struct IPlayer {

    virtual ~IPlayer() = default;

    virtual void BeginTurn() = 0;
    virtual void BeginHandleDrawOffer() = 0;
    virtual void BeginHandleOpponentDeclinedDrawOffer() = 0;
    virtual void BeginErrorRecoveryTurn() = 0;
    virtual void BeginRetrospectiveTurn() = 0;

};
