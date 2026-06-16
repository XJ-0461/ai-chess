#pragma once

#include <algorithm>
#include <memory>

#include <so_5/all.hpp>

#include "Chess/Board.h"
#include "Chess/Move.h"
#include "Game/Play/IPlayer.hpp"
#include "Game/Play/RemoteAgentPlayer.hpp"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Utility/Random.hpp"

namespace chess::game::execution {

struct BlockingTask {
    std::function<void()> task{};
};

struct BeginSetupRequest {};

struct CheckSetup {};

struct PlayerMoveAction {
    Colour color;
    std::string algebraic_move_string{};
};

struct PlayerOfferDrawAction {
    Colour color;
};

struct PlayerAcceptDrawAction {
    Colour color;
};

struct PlayerDeclineDrawAction {
    Colour color;
};

struct PlayerSubmitResignationAction {
    Colour color;
};

struct PlayerMoveActionValidationSuccess {
    Colour color;
    std::string algebraic_move_string{};
};

struct PlayerMoveActionValidationFailure {
    Colour color;
    std::string algebraic_move_string{};
    std::vector<std::string> errors{};
};

struct PlayerQuip : std::string {
    std::string quip{};
};

struct PlayerYield : so_5::signal_t {};

struct PlayerFactory {};

struct AttachPlayer {
    Colour colour;
    std::shared_ptr<IPlayer> player;
};

struct BeginTurn {
    Colour color;
};

struct BeginErrorRecoveryTurn {};
struct BeginRetrospectiveTurn {};

struct StartGame {};
struct EndGame {};
struct TurnTransition {};

struct GameStateChanged {
    std::string game_id{};
    std::string state_query_name{};
};

struct ApplicationEnvironment {
    std::shared_ptr<PlayerFactory> player_factory{};
};

struct GameState {
    std::shared_ptr<Board> board;
};

struct PlayerState {
    std::shared_ptr<IPlayer> white{};
    std::shared_ptr<IPlayer> black{};
};

struct InteractionState {
    std::shared_ptr<Square> selected_piece;
    std::shared_ptr<BitBoard> legal_moves;
    bool is_holding_piece{false};
};

struct RetrospectiveState {
    std::size_t retrospective_action_counter{}; // divide by 2 to get the number of retrospective turns
};

struct GameOrchestratorState {
    ApplicationEnvironment environment{};
    GameConfiguration game_configuration{};
    PlayerState players{};
    GameState game_state{};
    InteractionState interaction_state{};
    RetrospectiveState retrospective_state{};
};

class GameOrchestrator : public so_5::agent_t {
public:

    explicit GameOrchestrator(
        context_t ctx,
        GameOrchestratorState state
    ) : so_5::agent_t(std::move(ctx)),
        state_{std::move(state)},
        game_id_{util::RandomAlphaString(7)} {}

    [[nodiscard]]
    std::string GetGameId() const {
        return game_id_;
    }

protected:

    void so_define_agent() override {

        game_state_events_ = so_environment().create_mbox("GameOrchestrator.StateChange");

        so_change_state(initial);
        so_subscribe_self()
            .in(initial).event(&GameOrchestrator::OnGameConfiguration)
            .in(configured).event(&GameOrchestrator::OnBeginSetup)
            .in(configured).event(&GameOrchestrator::OnAttachPlayer)
            .in(configured).event(&GameOrchestrator::OnCheckSetup)
            .in(ready).event(&GameOrchestrator::OnStartGame)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnBeginTurn)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerMoveAction)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerOfferDrawAction)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerAcceptDrawAction)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerDeclineDrawAction)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerSubmitResignationAction)
            .in(white_action).in(black_action).event(&GameOrchestrator::OnPlayerQuip)
            .in(white_action).in(black_turn).event(&GameOrchestrator::OnPlayerYield)
            .in(white_action).in(black_action).event(&GameOrchestrator::PerformPlayerMoveAction)
            .in(white_action).in(black_action).event(&GameOrchestrator::PerformPlayerOfferDrawAction)
            .in(white_action).in(black_action).event(&GameOrchestrator::PerformPlayerAcceptDrawAction)
            .in(white_action).in(black_action).event(&GameOrchestrator::PerformPlayerDeclineDrawAction)
            .in(white_action).in(black_action).event(&GameOrchestrator::PerformPlayerSubmitResignationAction)
            .in(concluded).event(&GameOrchestrator::OnEndGame)
            .in(white_retrospective).in(black_retrospective).event(&GameOrchestrator::OnBeginRetrospectiveTurn)
            .in(white_retrospective).in(black_retrospective).event(&GameOrchestrator::OnPlayerQuip);

        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
    }

private:

    // Actor States
    so_5::state_t initial{this};
    so_5::state_t configured{this};
    so_5::state_t ready{this};
    so_5::state_t in_progress{this};
    so_5::state_t white_turn{ initial_substate_of{in_progress}, "white_turn" };
    so_5::state_t black_turn{ substate_of{in_progress}, "black_turn" };
    so_5::state_t white_action{ substate_of{in_progress}, "white_action" };
    so_5::state_t black_action{ substate_of{in_progress}, "black_action" };
    so_5::state_t concluded{this};
    so_5::state_t white_retrospective{ substate_of{concluded}, "white_retrospective" };
    so_5::state_t black_retrospective{ substate_of{concluded}, "black_retrospective" };
    so_5::state_t terminating{this};

    std::string game_id_{};
    so_5::mbox_t game_state_events_{}; // Allow outside world to subscribe to events

    // Game State
    GameOrchestratorState state_;

    // players can only take one action per turn
    using ActionType = std::variant<
        std::monostate,
        PlayerMoveAction,
        PlayerOfferDrawAction,
        PlayerAcceptDrawAction,
        PlayerDeclineDrawAction,
        PlayerSubmitResignationAction
    >;

    ActionType handle_this_turn_{};

    struct NextTurnPayload {
        ActionType action_to_handle{}; // one of: PlayerMoveAction, PlayerOfferDrawAction, PlayerDeclineDrawAction
        std::vector<std::string> quips{};
        std::vector<std::string> errors{};
    } next_turn_payload_{};

    void OnGameConfiguration(const mhood_t<GameConfiguration> config) {
        state_.game_configuration = *config;
        so_change_state(configured);
        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
    }

    void OnBeginSetup(const mhood_t<BeginSetupRequest>) {
        state_.game_state.board = std::make_shared<Board>();
        state_.interaction_state = InteractionState{
            .selected_piece = nullptr,
            .legal_moves = nullptr,
            .is_holding_piece = false
        };

        const auto setup_white = [this]() -> void {
            player::RemoteAgentPlayerConfiguration config {
                .zmq_endpoint = state_.game_configuration.white_endpoint,
                .enable_quip = state_.game_configuration.enable_quip,
                .enable_draw_offer = state_.game_configuration.enable_draw_offer,
                .enable_resignation = state_.game_configuration.enable_resignation
            };
            const auto white = std::make_shared<player::RemoteAgentPlayer>(config);
            white->Connect();
            so_5::send<AttachPlayer>(*this, White, white);
        };

        const auto setup_black = [this]() -> void {
            player::RemoteAgentPlayerConfiguration config {
                .zmq_endpoint = state_.game_configuration.black_endpoint,
                .enable_quip = state_.game_configuration.enable_quip,
                .enable_draw_offer = state_.game_configuration.enable_draw_offer,
                .enable_resignation = state_.game_configuration.enable_resignation
            };
            const auto black = std::make_shared<player::RemoteAgentPlayer>(config);
            black->Connect();
            so_5::send<AttachPlayer>(*this, Black, black);
        };

        const so_5::mbox_t blocking_task = so_environment().create_mbox("BlockingTask");
        so_5::send<BlockingTask>(blocking_task, setup_white);
        so_5::send<BlockingTask>(blocking_task, setup_black);
    }

    void OnCheckSetup(const mhood_t<CheckSetup>) {
        const bool setup_is_valid =
            (state_.players.white != nullptr) &&
            (state_.players.black != nullptr) &&
            (state_.game_state.board != nullptr);
        if (setup_is_valid) {
            so_change_state(ready);
            so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
        }
    }

    void OnAttachPlayer(const mhood_t<AttachPlayer> message) {
        if (message->colour == White) {
            state_.players.white = message->player;
        } else {
            state_.players.black = message->player;
        }
        so_5::send<CheckSetup>(*this);
    }

    void OnStartGame(const mhood_t<StartGame>) {
        so_change_state(in_progress);
        so_5::send<BeginTurn>(*this, White);
        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
    }

    // signal the player's subsystem to begin their turn
    void OnBeginTurn(const mhood_t<BeginTurn>) const {
        std::shared_ptr<IPlayer> player{};
        if (so_current_state().query_name() == "white_turn") {
            player = state_.players.white;
        } else {
            player = state_.players.black;
        }

        // 'next_turn_payload_' is now "THIS_turn_payload_"
        if (std::holds_alternative<PlayerMoveAction>(next_turn_payload_.action_to_handle)) {
            player->BeginTurn();
        } else if (std::holds_alternative<PlayerOfferDrawAction>(next_turn_payload_.action_to_handle)) {
            player->BeginHandleDrawOffer();
        } else if (std::holds_alternative<PlayerDeclineDrawAction>(next_turn_payload_.action_to_handle)) {
            player->BeginHandleOpponentDeclinedDrawOffer();
        } else {
            const std::string error_message = "Action to handle on " + so_current_state().query_name() + " is an invalid option";
            throw std::runtime_error{error_message};
        }

    }

    // this is when we want to kick it back to the same player because a recoverable error happened during their turn
    // we dont want to increment the game state or anything here, just ask them to retry
    void OnBeginErrorRecoveryTurn(const mhood_t<BeginErrorRecoveryTurn>) const {
        // signal the player's subsystem to begin their turn
        if (so_current_state().query_name() == "white_turn") {
            state_.players.white->BeginErrorRecoveryTurn();
        } else {
            state_.players.black->BeginErrorRecoveryTurn();
        }
    }

    void OnPlayerMoveAction(const mhood_t<PlayerMoveAction> move) {
        AssertNextActionUnset();
        handle_this_turn_ = *move;
    }

    // todo: update this to be more robust with respect to the move handling
    void PerformPlayerMoveAction(const mhood_t<PlayerMoveAction> move) {
        const auto blocking_mbox = so_environment().create_mbox("BlockingTask");
        const auto validate_move_task = [this, move]() -> void {
            bool success = state_.game_state.board->IsMoveLegal(LongAlgebraicMove{move->algebraic_move_string});
            if (!success) {
                so_5::send<PlayerMoveActionValidationFailure>(
                    *this,
                    move->color,
                    move->algebraic_move_string,
                    std::vector<std::string>{}
                );
            }
            so_5::send<PlayerMoveActionValidationSuccess>(
                *this,
                move->color,
                move->algebraic_move_string
            );
        };
        so_5::send<BlockingTask>(blocking_mbox, validate_move_task);
    }

    void OnPlayerMoveActionValidation(const mhood_t<PlayerMoveActionValidationSuccess> sucess) {
        // todo: make the move on the board
        // todo: check if the game is over
        const bool is_game_over = false;
        if (is_game_over) {
            so_change_state(concluded);
            so_5::send<EndGame>(*this);
            so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
        }
        so_5::send<TurnTransition>(*this);
    }

    void OnPlayerMoveActionValidation(const mhood_t<PlayerMoveActionValidationFailure> failure) {
        next_turn_payload_.errors.append_range(failure->errors);
        so_5::send<BeginErrorRecoveryTurn>(*this);
    }

    void OnPlayerQuip(const mhood_t<PlayerQuip> quip) {
        next_turn_payload_.quips.emplace_back(quip->quip);
    }

    void OnPlayerOfferDrawAction(const mhood_t<PlayerOfferDrawAction> offer_draw) {
        AssertNextActionUnset();
        handle_this_turn_ = *offer_draw;
    }

    void PerformPlayerOfferDrawAction(const mhood_t<PlayerOfferDrawAction> offer_draw) {
        next_turn_payload_.action_to_handle = handle_this_turn_;
        handle_this_turn_ = std::monostate{};
        so_5::send<TurnTransition>(*this);
    }

    void OnPlayerAcceptDrawAction(const mhood_t<PlayerAcceptDrawAction> accept_draw) {
        AssertNextActionUnset();
        handle_this_turn_ = *accept_draw;
    }

    void PerformPlayerAcceptDrawAction(const mhood_t<PlayerAcceptDrawAction> accept_draw) {
        // todo: set the game to a draw ("freeze" the game or something, or prevent the board from accepting any more moves)
        // then transition the game to the EndGame (concluded) state
        so_5::send<EndGame>(*this);
    }

    void OnPlayerDeclineDrawAction(const mhood_t<PlayerDeclineDrawAction> decline_draw) {
        AssertNextActionUnset();
        handle_this_turn_ = *decline_draw;
    }

    void PerformPlayerDeclineDrawAction(const mhood_t<PlayerDeclineDrawAction> decline_draw) {
        next_turn_payload_.action_to_handle = handle_this_turn_;
        handle_this_turn_ = std::monostate{};
        so_5::send<TurnTransition>(*this);
    }

    void OnPlayerSubmitResignationAction(const mhood_t<PlayerSubmitResignationAction> submit_resignation) {
        AssertNextActionUnset();
        handle_this_turn_ = *submit_resignation;
    }

    void PerformPlayerSubmitResignationAction(const mhood_t<PlayerSubmitResignationAction> submit_resignation) {
        // todo: set the game to a draw ("freeze" the game or something, or prevent the board from accepting any more moves)
        // then transition the game to the EndGame (concluded) state
        handle_this_turn_ = std::monostate{};
        so_5::send<TurnTransition>(*this);
    }

    // yield tells the system that all the accumulating messages are done
    // handle the action with the accumulated state/status (quips, etc.)
    // after the handler is finished
    // acts as a dumb router
    void OnPlayerYield(const mhood_t<PlayerYield>) {

        if (so_current_state().query_name() == "white_turn") {
            so_change_state(white_action);
        } else {
            so_change_state(black_action);
        }
        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());

        std::visit([this](auto&& action) -> void {

            const auto blocking_mbox = so_environment().create_mbox("BlockingTask");

            using ActionT = std::decay_t<decltype(action)>;
            if constexpr (std::same_as<ActionT, PlayerMoveAction>) {
                so_5::send<PlayerMoveAction>(*this, std::forward<PlayerMoveAction>(action));
            } else if constexpr (std::same_as<ActionT, PlayerOfferDrawAction>) {
                so_5::send<PlayerOfferDrawAction>(*this, std::forward<PlayerOfferDrawAction>(action));
            } else if constexpr (std::same_as<ActionT, PlayerAcceptDrawAction>) {
                so_5::send<PlayerAcceptDrawAction>(*this, std::forward<PlayerAcceptDrawAction>(action));
            } else if constexpr (std::same_as<ActionT, PlayerDeclineDrawAction>) {
                so_5::send<PlayerDeclineDrawAction>(*this, std::forward<PlayerDeclineDrawAction>(action));
            } else if constexpr (std::same_as<ActionT, PlayerSubmitResignationAction>) {
                so_5::send<PlayerSubmitResignationAction>(*this, std::forward<PlayerSubmitResignationAction>(action));
            } else if constexpr (std::same_as<ActionT, std::monostate>) {
                // Error: player didn't submit an action, need to send it back to the agent to try again
            } else {
                static_assert(false, "Non-exhaustive visitor!");
            }
        }, handle_this_turn_);

    }

    void OnTurnTransition(const mhood_t<TurnTransition>) {
        const auto& current_state = so_current_state();
        if (current_state.query_name() == "white_action") {
            so_change_state(black_turn);
        } else if (current_state.query_name() == "black_action") {
            so_change_state(white_turn);
        }
        so_5::send<BeginTurn>(*this);
        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
    }

    void OnEndGame(const mhood_t<EndGame>) {
        if (state_.game_configuration.retrospective_turn_count > 0) {
            if (so_current_state().query_name() == "white_action") {
                so_change_state(black_retrospective);
            } else {
                so_change_state(white_retrospective);
            }
            so_5::send<BeginRetrospectiveTurn>(*this);
            so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
        } else {
            so_change_state(concluded);
            so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
        }
    }

    void OnBeginRetrospectiveTurn(const mhood_t<BeginRetrospectiveTurn>) {
        const auto& player = GetPlayerForThisTurn();
        player->BeginRetrospectiveTurn();
    }

    // if the agent queues two actions in one turn its an error!
    void AssertNextActionUnset() const {
        if (std::holds_alternative<std::monostate>(handle_this_turn_)) {
            return;
        }
        const auto& current_state = so_current_state();
        std::string error_message{};
        if (current_state.query_name().contains("white")) {
            error_message = "White player tried to queue more than one action on its turn";
        } else {
            error_message = "Black player tried to queue more than one action on its turn";
        }
        throw std::runtime_error{error_message};
    }

    [[nodiscard]]
    std::shared_ptr<IPlayer> GetPlayerForThisTurn() const {
        if (so_current_state().query_name().contains("white")) {
            return state_.players.white;
        }
        if (so_current_state().query_name().contains("black")) {
            return state_.players.black;
        }
        throw std::runtime_error{"GetPlayerForThisTurn() is not supported for this state"};
    }

};

} // namespace chess::game::execution
