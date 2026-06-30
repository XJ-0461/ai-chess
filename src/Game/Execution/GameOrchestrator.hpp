#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <so_5/all.hpp>

#include "Chess/Board.h"
#include "Chess/Move.h"
#include "Chess/ChessException.h"
#include "Game/Error/AgentMoveError.hpp"
#include "Game/Play/IPlayer.hpp"
#include "Game/Play/RemoteAgentPlayer.hpp"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Utility/Random.hpp"

#include "Application/Task/BlockingTask.hpp"
#include "Game/Execution/Message/Lifecycle.hpp"
#include "Game/Execution/Message/PlayerAction.hpp"
#include "Game/Execution/Message/MoveValidation.hpp"
#include "Game/Execution/Message/Notification.hpp"
#include "Game/Execution/State.hpp"

#include "Utility/Log/Switch.hpp"

namespace chess::game::execution {

class GameOrchestrator : public so_5::agent_t {
public:

    explicit GameOrchestrator(
        context_t ctx,
        GameOrchestratorState state
    ) : so_5::agent_t(std::move(ctx)),
        state_{std::move(state)} {
        game_id_ = state_.game_id.empty() ? util::RandomAlphaString(7) : state_.game_id;
    }

    [[nodiscard]]
    std::string GetGameId() const {
        return game_id_;
    }

protected:

    void so_define_agent() override {

        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "so_define_agent"}, {"game_id", game_id_});

        game_state_events_ = so_environment().create_mbox("GameOrchestrator.StateChange");

        so_change_state(initial);

        // NOTE: a subscription_bind accumulates every .in() state across the
        // whole chain (states are not reset between .event() calls), so each
        // distinct state-set gets its own so_subscribe_self() statement to
        // avoid subscribing a message to the same state twice.
        so_subscribe_self().in(initial)
            .event(&GameOrchestrator::OnGameConfiguration);

        so_subscribe_self().in(configured)
            .event(&GameOrchestrator::OnBeginSetup)
            .event(&GameOrchestrator::OnAttachPlayer)
            .event(&GameOrchestrator::OnCheckSetup);

        so_subscribe_self().in(ready)
            .event(&GameOrchestrator::OnStartGame);

        // Turn phase: a player is choosing/queuing their action (the handlers
        // inspect the "white_turn"/"black_turn" state name).
        so_subscribe_self().in(white_turn).in(black_turn)
            .event(&GameOrchestrator::OnBeginTurn)
            .event(&GameOrchestrator::OnBeginErrorRecoveryTurn)
            .event(&GameOrchestrator::OnPlayerMoveAction)
            .event(&GameOrchestrator::OnPlayerOfferDrawAction)
            .event(&GameOrchestrator::OnPlayerAcceptDrawAction)
            .event(&GameOrchestrator::OnPlayerDeclineDrawAction)
            .event(&GameOrchestrator::OnPlayerSubmitResignationAction)
            .event(&GameOrchestrator::OnPlayerQuip)
            .event(&GameOrchestrator::OnPlayerYield)
            .event(&GameOrchestrator::OnPlayerError);

        // Action phase: the queued action is processed and the result drives the
        // board update + turn transition.
        so_subscribe_self().in(white_action).in(black_action)
            .event(&GameOrchestrator::PerformPlayerMoveAction)
            .event(&GameOrchestrator::PerformPlayerOfferDrawAction)
            .event(&GameOrchestrator::PerformPlayerAcceptDrawAction)
            .event(&GameOrchestrator::PerformPlayerDeclineDrawAction)
            .event(&GameOrchestrator::PerformPlayerSubmitResignationAction)
            .event(&GameOrchestrator::OnMoveValidationSuccess)
            .event(&GameOrchestrator::OnMoveValidationFailure)
            .event(&GameOrchestrator::OnTurnTransition)
            // EndGame is raised from an action state (checkmate / draw accepted),
            // and OnEndGame needs that state to pick the retrospective side.
            .event(&GameOrchestrator::OnEndGame);

        so_subscribe_self().in(white_retrospective).in(black_retrospective)
            .event(&GameOrchestrator::OnBeginRetrospectiveTurn)
            .event(&GameOrchestrator::OnPlayerQuip)
            .event(&GameOrchestrator::OnRetrospectiveYield);

        PublishStateChange();
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
    // concluded is composite (it parents the retrospective states), so it needs
    // an initial substate to be entered directly (e.g. on forfeit / a game that
    // ends without retrospective). game_over is the terminal, idle leaf.
    so_5::state_t game_over{ initial_substate_of{concluded}, "game_over" };
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

    // Consecutive failed attempts (illegal move or agent error) on the current
    // turn. Reset on a successful move; a persistently-failing player forfeits
    // rather than looping forever.
    static constexpr std::size_t kMaxTurnRetries = 15;
    std::size_t consecutive_failures_{0};

    // The result determined at the point a game ends, carried to OnEndGame where
    // it is finalized (forfeits finalize directly and don't route through here).
    MatchResult pending_result_{};

    // Maps the active actor state to a coarse lifecycle phase using state
    // identity (so_current_state().query_name() returns a fully-qualified
    // hierarchical name, so string comparisons against bare names are unsafe).
    [[nodiscard]] GameLifecyclePhase CurrentPhase() const {
        if (so_is_active_state(concluded)) {
            return GameLifecyclePhase::Concluded; // includes the retrospective substates
        }
        if (so_is_active_state(in_progress)) {
            return GameLifecyclePhase::InProgress; // includes the turn/action substates
        }
        if (so_is_active_state(ready)) {
            return GameLifecyclePhase::Ready;
        }
        if (so_is_active_state(configured)) {
            return GameLifecyclePhase::Configured;
        }
        if (so_is_active_state(terminating)) {
            return GameLifecyclePhase::Terminating;
        }
        return GameLifecyclePhase::Initial;
    }

    // Publishes the current state both as an event (for mbox subscribers) and
    // into the shared phase sink (for pollers like the UI command runner).
    void PublishStateChange() {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "publish_state"}, {"function", "PublishStateChange"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        so_5::send<GameStateChanged>(game_state_events_, game_id_, so_current_state().query_name());
        if (state_.published_phase) {
            state_.published_phase->store(CurrentPhase());
        }
    }

    // Records the decisive result into the shared sink (for external queries)
    // and each player's trajectory (for the sidebar score). Must run before the
    // Concluded phase is published, so observers that wake on Concluded see it.
    void FinalizeResult(const MatchResult& result) {
        pending_result_ = result;
        if (state_.published_result) {
            *state_.published_result = result;
        }
        if (state_.white_trajectory) {
            state_.white_trajectory->SetResultScore(ScoreString(result.outcome, White));
        }
        if (state_.black_trajectory) {
            state_.black_trajectory->SetResultScore(ScoreString(result.outcome, Black));
        }
        CHESS_TRACE_LOG("stdout_chess", {"operation", "finalize_result"}, {"function", "FinalizeResult"}, {"game_id", game_id_}, {"outcome", std::string(OutcomeToString(result.outcome))}, {"cause", result.cause});

        // End player and game telemetry contexts
        if (state_.gaunt_context) {
            namespace ggcc = gaunt::generated::chess_coliseum;

            if (state_.gaunt_context->white_player.player_context.has_value()) {
                state_.gaunt_context->white_player.player_context->Accept(ggcc::event::PlayerEnd{});
                state_.gaunt_context->white_player.player_context.reset();
            }
            if (state_.gaunt_context->black_player.player_context.has_value()) {
                state_.gaunt_context->black_player.player_context->Accept(ggcc::event::PlayerEnd{});
                state_.gaunt_context->black_player.player_context.reset();
            }

            if (state_.gaunt_context->game_context.has_value()) {
                state_.gaunt_context->game_context->Accept(ggcc::event::GameEnd{});
                state_.gaunt_context->game_context.reset();
            }
        }
    }

    void OnGameConfiguration(const mhood_t<GameConfiguration> config) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnGameConfiguration"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        state_.game_configuration = *config;
        so_change_state(configured);
        PublishStateChange();
    }

    void OnBeginSetup(const mhood_t<BeginSetupRequest>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnBeginSetup"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        // Keep the board injected at construction (shared with views); only
        // create one if none was provided.
        if (!state_.game_state.board) {
            state_.game_state.board = std::make_shared<Board>();
        }

        // Snapshot everything the setup tasks need by VALUE so the task threads
        // never touch state_ concurrently with the orchestrator.
        const so_5::mbox_t self_mbox = so_direct_mbox();
        const auto board = state_.game_state.board;
        const auto board_mutex = state_.game_state.board_mutex;
        const auto configuration = state_.game_configuration;
        const auto white_trajectory = state_.white_trajectory;
        const auto black_trajectory = state_.black_trajectory;

        const auto make_config = [configuration](const chess::game::AgentConfiguration& agent, const char* color) {
            return player::RemoteAgentPlayerConfiguration{
                .zmq_endpoint = agent.endpoint,
                .color = color,
                .provider = chess::game::AgentProviderLabel(agent.provider),
                .model = agent.model_id,
                .api_key = agent.api_key,
                .enable_quip = configuration.enable_quip,
                .enable_draw_offer = configuration.enable_draw_offer,
                .enable_resignation = configuration.enable_resignation,
            };
        };

        const auto setup_white = [=]() -> void {
            const auto white = std::make_shared<player::RemoteAgentPlayer>(
                make_config(configuration.white, "WHITE"), White, self_mbox, white_trajectory, board, board_mutex);
            white->Connect();
            so_5::send<AttachPlayer>(self_mbox, White, white);
        };

        const auto setup_black = [=]() -> void {
            const auto black = std::make_shared<player::RemoteAgentPlayer>(
                make_config(configuration.black, "BLACK"), Black, self_mbox, black_trajectory, board, board_mutex);
            black->Connect();
            so_5::send<AttachPlayer>(self_mbox, Black, black);
        };

        const so_5::mbox_t blocking_task = so_environment().create_mbox(std::string{chess::application::task::kBlockingTaskMboxName});
        so_5::send<chess::application::task::BlockingTask>(blocking_task, setup_white);
        so_5::send<chess::application::task::BlockingTask>(blocking_task, setup_black);
    }

    void OnCheckSetup(const mhood_t<CheckSetup>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnCheckSetup"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        const bool setup_is_valid =
            (state_.players.white != nullptr) &&
            (state_.players.black != nullptr) &&
            (state_.game_state.board != nullptr);
        if (setup_is_valid) {
            so_change_state(ready);
            PublishStateChange();
        }
    }

    void OnAttachPlayer(const mhood_t<AttachPlayer> message) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnAttachPlayer"}, {"game_id", game_id_}, {"colour", message->colour == White ? "white" : "black"});
        if (message->colour == White) {
            state_.players.white = message->player;
        } else {
            state_.players.black = message->player;
        }

        // Emit player start telemetry
        if (state_.gaunt_context && state_.gaunt_context->game_context.has_value()) {
            namespace ggcc = gaunt::generated::chess_coliseum;
            auto& player_ctx = (message->colour == White)
                ? state_.gaunt_context->white_player
                : state_.gaunt_context->black_player;

            const auto gaunt_color = chess::telemetry::ToGauntColor(message->colour);
            player_ctx.player_context = state_.gaunt_context->game_context->CreateChildContext<
                ggcc::context::Player>(
                ggcc::context::Player::Key{gaunt_color},
                ggcc::event::PlayerStart{}
            );
        }

        so_5::send<CheckSetup>(*this);
    }

    void OnStartGame(const mhood_t<StartGame>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnStartGame"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        // Enter white_turn explicitly: relying on in_progress's initial substate
        // is fragile, and if the current state reads as "in_progress" then
        // OnBeginTurn's fallback would (incorrectly) pick black. White moves first.
        so_change_state(white_turn);
        so_5::send<BeginTurn>(*this, White);
        PublishStateChange();
    }

    // Builds the up-to-date context (move history, accumulated quips/errors,
    // full-move number) that a player needs to send a complete request.
    [[nodiscard]] TurnContext MakeTurnContext() const {
        TurnContext context;
        context.game_history = state_.game_state.move_history;
        context.opponent_quips = next_turn_payload_.quips;
        context.errors = next_turn_payload_.errors;
        context.turn_number = static_cast<std::uint32_t>(state_.game_state.move_history.size() / 2 + 1);
        // Include the player's own quip history for self-context (avoid repetition)
        const bool is_white = so_is_active_state(white_turn) || so_is_active_state(white_action) || so_is_active_state(white_retrospective);
        context.own_quip_history = is_white ? state_.game_state.white_quip_history : state_.game_state.black_quip_history;
        return context;
    }

    // signal the player's subsystem to begin their turn
    void OnBeginTurn(const mhood_t<BeginTurn>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnBeginTurn"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        std::shared_ptr<IPlayer> player = so_is_active_state(white_turn) ? state_.players.white : state_.players.black;
        const TurnContext context = MakeTurnContext();

        // Create Move telemetry context
        if (state_.gaunt_context) {
            namespace ggcc = gaunt::generated::chess_coliseum;
            auto& player_ctx = so_is_active_state(white_turn)
                ? state_.gaunt_context->white_player
                : state_.gaunt_context->black_player;

            if (player_ctx.player_context.has_value()) {
                player_ctx.move_counter = player_ctx.move_counter + 1;
                player_ctx.current_move_context = player_ctx.player_context->CreateChildContext<
                    ggcc::context::Move>(
                    ggcc::context::Move::Key{player_ctx.move_counter},
                    ggcc::event::MoveBegin{}
                );
            }
        }

        // A queued draw offer / declined-draw asks this player to handle that
        // specific follow-up; anything else - including the first turn's
        // std::monostate or a normal queued move - is a normal move turn.
        if (std::holds_alternative<PlayerOfferDrawAction>(next_turn_payload_.action_to_handle)) {
            player->BeginHandleDrawOffer(context);
        } else if (std::holds_alternative<PlayerDeclineDrawAction>(next_turn_payload_.action_to_handle)) {
            player->BeginHandleOpponentDeclinedDrawOffer(context);
        } else {
            player->BeginTurn(context);
        }
    }

    // this is when we want to kick it back to the same player because a recoverable error happened during their turn
    // we dont want to increment the game state or anything here, just ask them to retry
    void OnBeginErrorRecoveryTurn(const mhood_t<BeginErrorRecoveryTurn>) const {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnBeginErrorRecoveryTurn"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        std::shared_ptr<IPlayer> player = so_is_active_state(white_turn) ? state_.players.white : state_.players.black;
        player->BeginErrorRecoveryTurn(MakeTurnContext());
    }

    void OnPlayerMoveAction(const mhood_t<PlayerMoveAction> move) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerMoveAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"move", move->algebraic_move_string});
        AssertNextActionUnset();
        handle_this_turn_ = *move;
    }

    // Validates the submitted move off-thread (board reads are guarded), then
    // reports success or failure back to the orchestrator.
    void PerformPlayerMoveAction(const mhood_t<PlayerMoveAction> move) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "PerformPlayerMoveAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"move", move->algebraic_move_string});
        const auto blocking_mbox = so_environment().create_mbox(std::string{chess::application::task::kBlockingTaskMboxName});
        const auto board = state_.game_state.board;
        const auto board_mutex = state_.game_state.board_mutex;
        const auto validate_move_task = [this, move, board, board_mutex]() -> void {
            bool legal = false;
            std::vector<chess::game::error::MoveError> errors;

            if (board) {
                // Agents speak SAN ("e5", "Nf3", "O-O", ...). Validate on a copy
                // so the real board is untouched; Board::Move(AlgebraicMove)
                // resolves + applies the SAN and throws if illegal/ambiguous.
                Board snapshot;
                if (board_mutex) {
                    const std::lock_guard<std::mutex> lock(*board_mutex);
                    snapshot = *board;
                } else {
                    snapshot = *board;
                }

                try {
                    snapshot.Move(AlgebraicMove{move->algebraic_move_string});
                    legal = true;
                } catch (const chess::game::error::MoveErrorException& e) {
                    errors.push_back(e.error);
                } catch (const IllegalMoveException& e) {
                    errors.push_back(chess::game::error::ClassifyIllegalMove(e.what(), move->algebraic_move_string));
                } catch (const InvalidAlgebraicMoveException&) {
                    errors.push_back(chess::game::error::InvalidNotation{move->algebraic_move_string}.error);
                } catch (const InvalidCaptureException& e) {
                    errors.push_back(chess::game::error::InvalidCaptureNotation{e.move()}.error);
                } catch (const std::exception& e) {
                    errors.push_back({chess::game::error::kErrorAgentFailure, e.what()});
                }
            }

            if (!legal) {
                so_5::send<PlayerMoveActionValidationFailure>(
                    *this, move->color, move->algebraic_move_string, move->id, errors);
                return;
            }
            so_5::send<PlayerMoveActionValidationSuccess>(
                *this, move->color, move->algebraic_move_string, move->id);
        };
        so_5::send<chess::application::task::BlockingTask>(blocking_mbox, validate_move_task);
    }

    void OnMoveValidationSuccess(const mhood_t<PlayerMoveActionValidationSuccess> success) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnMoveValidationSuccess"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"move", success->algebraic_move_string});
        bool applied = false;
        bool is_game_over = false;
        Piece captured_piece = None;
        MatchOutcome end_outcome = MatchOutcome::InProgress;
        std::string end_cause{};

        const auto count_pieces = [this]() {
            std::array<int, 16> counts{};
            for (int s = 0; s < 64; ++s) {
                ++counts[static_cast<std::size_t>((*state_.game_state.board)[static_cast<Square>(s)])];
            }
            return counts;
        };

        const auto apply_move = [&]() {
            try {
                const std::array<int, 16> before = count_pieces();

                // Re-resolve the SAN against the (unchanged) real board and apply.
                state_.game_state.board->Move(AlgebraicMove{success->algebraic_move_string});
                applied = true;
                // Record the move so subsequent turn requests carry the full history.
                state_.game_state.move_history.push_back(success->algebraic_move_string);

                // Detect the captured piece (if any) by diffing only the
                // opponent's per-type counts: exactly one drops by 1 on a
                // capture (incl. en passant); promotion only touches the mover's
                // own counts, so it can't be mistaken for a capture.
                const std::array<int, 16> after = count_pieces();
                const Colour opponent = OppositeColour(success->color);
                for (int t = 0; t < PieceTypeCount; ++t) {
                    const auto opp_piece = static_cast<Piece>((opponent << 3) | t);
                    if (before[opp_piece] - after[opp_piece] == 1) {
                        captured_piece = opp_piece;
                        break;
                    }
                }

                const Colour to_move = state_.game_state.board->GetPlayerTurn();
                is_game_over = !state_.game_state.board->HasLegalMoves(to_move);
                if (is_game_over) {
                    if (state_.game_state.board->IsInCheck(to_move)) {
                        end_outcome = (success->color == White) ? MatchOutcome::WhiteWin : MatchOutcome::BlackWin;
                        end_cause = "checkmate";
                    } else {
                        end_outcome = MatchOutcome::Draw;
                        end_cause = "stalemate";
                    }
                }
            } catch (const std::exception&) {
                applied = false;
            }
        };

        if (state_.game_state.board) {
            if (state_.game_state.board_mutex) {
                const std::lock_guard<std::mutex> lock(*state_.game_state.board_mutex);
                apply_move();
            } else {
                apply_move();
            }
        }

        // The board changed; notify observers (phase string is unchanged, but
        // views poll the board itself).
        PublishStateChange();

        if (!applied) {
            CHESS_TRACE_LOG("stdout_chess", {"operation", "branch"}, {"function", "OnMoveValidationSuccess"}, {"game_id", game_id_}, {"outcome", "apply_failed"}, {"move", success->algebraic_move_string});
            // Shouldn't happen post-validation; recover by re-prompting the player.
            so_5::send<PlayerMoveActionValidationFailure>(
                *this, success->color, success->algebraic_move_string, success->id,
                std::vector<chess::game::error::MoveError>{{chess::game::error::kErrorMoveNotApplied, "Move could not be applied to the board."}});
            return;
        }

        consecutive_failures_ = 0; // a legal move landed; clear the failure streak

        // Record the accepted move in the shared log (sidebar bubble + history bar).
        if (state_.move_log) {
            state_.move_log->Append({success->id, success->color, success->algebraic_move_string, true, {}});
        }

        // Record the capture in the mover's trajectory (opponent piece, in order).
        if (captured_piece != None) {
            const auto& mover_trajectory = (success->color == White) ? state_.white_trajectory : state_.black_trajectory;
            if (mover_trajectory) {
                mover_trajectory->AddCapturedPiece(captured_piece);
            }
            CHESS_TRACE_LOG("stdout_chess", {"operation", "capture"}, {"function", "OnMoveValidationSuccess"}, {"game_id", game_id_}, {"by", success->color == White ? "white" : "black"}, {"piece", static_cast<int>(captured_piece)});
        }

        // Emit successful move telemetry
        if (state_.gaunt_context) {
            namespace ggcc = gaunt::generated::chess_coliseum;
            auto& player_ctx = (success->color == White)
                ? state_.gaunt_context->white_player
                : state_.gaunt_context->black_player;

            if (player_ctx.current_move_context.has_value()) {
                const auto gaunt_color = chess::telemetry::ToGauntColor(success->color);
                player_ctx.current_move_context->Accept(
                    ggcc::event::MoveSubmitted{gaunt_color, success->algebraic_move_string}
                );
                player_ctx.current_move_context->Accept(ggcc::event::MoveEnd{});
                player_ctx.current_move_context.reset();
            }
        }

        if (is_game_over) {
            CHESS_TRACE_LOG("stdout_chess", {"operation", "branch"}, {"function", "OnMoveValidationSuccess"}, {"game_id", game_id_}, {"outcome", end_cause});
            pending_result_ = MatchResult{end_outcome, end_cause};
            so_5::send<EndGame>(*this);
            return;
        }

        next_turn_payload_ = {}; // the next player takes a normal turn
        so_5::send<TurnTransition>(*this);
    }

    void OnMoveValidationFailure(const mhood_t<PlayerMoveActionValidationFailure> failure) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnMoveValidationFailure"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"move", failure->algebraic_move_string});
        // Record the rejected attempt + its codified errors in the shared log.
        if (state_.move_log) {
            state_.move_log->Append({failure->id, failure->color, failure->algebraic_move_string, false, failure->errors});
        }
        // Surface the codified errors (CODE - description) to the agent's recovery context.
        for (const auto& move_error : failure->errors) {
            next_turn_payload_.errors.push_back(move_error.Format());
        }
        next_turn_payload_.errors.emplace_back("Move '" + failure->algebraic_move_string + "' is not legal.");

        // Emit failed move telemetry
        if (state_.gaunt_context) {
            namespace ggcc = gaunt::generated::chess_coliseum;
            auto& player_ctx = (failure->color == White)
                ? state_.gaunt_context->white_player
                : state_.gaunt_context->black_player;

            if (player_ctx.current_move_context.has_value()) {
                std::string validation_errors;
                for (const auto& err : failure->errors) {
                    if (!validation_errors.empty()) validation_errors += "; ";
                    validation_errors += err.Format();
                }
                player_ctx.current_move_context->Accept(
                    ggcc::event::MakeMove{failure->algebraic_move_string, validation_errors}
                );
                player_ctx.current_move_context->Accept(ggcc::event::MoveEnd{});
                player_ctx.current_move_context.reset();
            }
        }

        handle_this_turn_ = std::monostate{};

        // Kick back to the same player's turn before retrying.
        if (so_is_active_state(white_action)) {
            so_change_state(white_turn);
        } else {
            so_change_state(black_turn);
        }
        RetryOrForfeit();
    }

    // The agent itself failed to produce an action (e.g. finished without a
    // move). We're already in the player's turn state, so just recover/forfeit.
    void OnPlayerError(const mhood_t<PlayerError> error) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerError"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"message", error->message});
        next_turn_payload_.errors.emplace_back(error->message);
        RetryOrForfeit();
    }

    // Re-prompt the same player, or conclude the game if it has failed too many
    // times in a row (avoids an unbounded retry loop / softlock).
    void RetryOrForfeit() {
        ++consecutive_failures_;
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "RetryOrForfeit"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"consecutive_failures", consecutive_failures_}, {"max_retries", kMaxTurnRetries});
        if (consecutive_failures_ >= kMaxTurnRetries) {
            CHESS_TRACE_LOG("stdout_chess", {"operation", "branch"}, {"function", "RetryOrForfeit"}, {"game_id", game_id_}, {"outcome", "forfeit"}, {"consecutive_failures", consecutive_failures_});
            // A persistently-failing agent is a technical glitch, not a loss:
            // the game is void (NO_CONTEST). Finalize before publishing Concluded.
            FinalizeResult(MatchResult{MatchOutcome::NoContest, "forfeit_no_move"});
            so_change_state(concluded);
            PublishStateChange();
            return;
        }
        PublishStateChange();
        so_5::send<BeginErrorRecoveryTurn>(*this);
    }

    void OnPlayerQuip(const mhood_t<PlayerQuip> quip) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerQuip"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        next_turn_payload_.quips.emplace_back(quip->quip);
        // Also record in the player's own quip history for self-context
        if (so_is_active_state(white_turn) || so_is_active_state(white_action) || so_is_active_state(white_retrospective)) {
            state_.game_state.white_quip_history.emplace_back(quip->quip);
        } else {
            state_.game_state.black_quip_history.emplace_back(quip->quip);
        }
    }

    void OnPlayerOfferDrawAction(const mhood_t<PlayerOfferDrawAction> offer_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerOfferDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        AssertNextActionUnset();
        handle_this_turn_ = *offer_draw;
    }

    void PerformPlayerOfferDrawAction(const mhood_t<PlayerOfferDrawAction> offer_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "PerformPlayerOfferDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        next_turn_payload_.action_to_handle = handle_this_turn_;
        handle_this_turn_ = std::monostate{};
        so_5::send<TurnTransition>(*this);
    }

    void OnPlayerAcceptDrawAction(const mhood_t<PlayerAcceptDrawAction> accept_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerAcceptDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        AssertNextActionUnset();
        handle_this_turn_ = *accept_draw;
    }

    void PerformPlayerAcceptDrawAction(const mhood_t<PlayerAcceptDrawAction> accept_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "PerformPlayerAcceptDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        // A draw offer was accepted: the game ends as an agreed draw. OnEndGame
        // finalizes pending_result_ and concludes (no more moves are processed).
        pending_result_ = MatchResult{MatchOutcome::Draw, "draw_agreement"};
        so_5::send<EndGame>(*this);
    }

    void OnPlayerDeclineDrawAction(const mhood_t<PlayerDeclineDrawAction> decline_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerDeclineDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        AssertNextActionUnset();
        handle_this_turn_ = *decline_draw;
    }

    void PerformPlayerDeclineDrawAction(const mhood_t<PlayerDeclineDrawAction> decline_draw) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "PerformPlayerDeclineDrawAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        next_turn_payload_.action_to_handle = handle_this_turn_;
        handle_this_turn_ = std::monostate{};
        so_5::send<TurnTransition>(*this);
    }

    void OnPlayerSubmitResignationAction(const mhood_t<PlayerSubmitResignationAction> submit_resignation) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerSubmitResignationAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        AssertNextActionUnset();
        handle_this_turn_ = *submit_resignation;
    }

    void PerformPlayerSubmitResignationAction(const mhood_t<PlayerSubmitResignationAction> submit_resignation) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "PerformPlayerSubmitResignationAction"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        // The player whose action is being processed is resigning; the opponent
        // wins. (We are in white_action / black_action for that player.)
        const bool white_resigned = so_is_active_state(white_action);
        pending_result_ = MatchResult{white_resigned ? MatchOutcome::BlackWin : MatchOutcome::WhiteWin, "resignation"};
        handle_this_turn_ = std::monostate{};
        so_5::send<EndGame>(*this);
    }

    // yield tells the system that all the accumulating messages are done
    // handle the action with the accumulated state/status (quips, etc.)
    // after the handler is finished
    // acts as a dumb router
    void OnPlayerYield(const mhood_t<PlayerYield>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnPlayerYield"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});

        if (so_is_active_state(white_turn)) {
            so_change_state(white_action);
        } else {
            so_change_state(black_action);
        }
        PublishStateChange();

        std::visit([this](auto&& action) -> void {

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
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnTurnTransition"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        handle_this_turn_ = std::monostate{}; // consumed; ready for the next turn
        if (so_is_active_state(white_action)) {
            so_change_state(black_turn);
        } else if (so_is_active_state(black_action)) {
            so_change_state(white_turn);
        }
        so_5::send<BeginTurn>(*this);
        PublishStateChange();
    }

    void OnEndGame(const mhood_t<EndGame>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnEndGame"}, {"game_id", game_id_}, {"state", so_current_state().query_name()}, {"retrospective_turn_count", state_.game_configuration.retrospective_turn_count});
        // Finalize the result (set by whichever path led here: checkmate/stalemate,
        // draw agreement, or resignation) before any state transition publishes
        // the Concluded phase.
        FinalizeResult(pending_result_);
        if (state_.game_configuration.retrospective_turn_count > 0) {
            if (so_is_active_state(white_action)) {
                so_change_state(black_retrospective);
            } else {
                so_change_state(white_retrospective);
            }
            so_5::send<BeginRetrospectiveTurn>(*this);
            PublishStateChange();
        } else {
            so_change_state(concluded);
            PublishStateChange();
        }
    }

    void OnBeginRetrospectiveTurn(const mhood_t<BeginRetrospectiveTurn>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnBeginRetrospectiveTurn"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});
        const auto& player = GetPlayerForThisTurn();
        player->BeginRetrospectiveTurn(MakeTurnContext());
    }

    void OnRetrospectiveYield(const mhood_t<PlayerYield>) {
        CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnRetrospectiveYield"}, {"game_id", game_id_}, {"state", so_current_state().query_name()});

        state_.retrospective_state.retrospective_action_counter = state_.retrospective_state.retrospective_action_counter + 1;
        const std::size_t completed_turns = state_.retrospective_state.retrospective_action_counter / 2;
        const std::size_t configured_turns = state_.game_configuration.retrospective_turn_count;

        CHESS_TRACE_LOG("stdout_chess", {"operation", "check_retrospective_progress"}, {"function", "OnRetrospectiveYield"}, {"game_id", game_id_}, {"completed_turns", completed_turns}, {"configured_turns", configured_turns});

        if (completed_turns >= configured_turns) {
            // All retrospective turns completed; conclude the game.
            so_change_state(game_over);
            PublishStateChange();
            return;
        }

        // Transition to the other player's retrospective turn.
        if (so_is_active_state(white_retrospective)) {
            so_change_state(black_retrospective);
        } else {
            so_change_state(white_retrospective);
        }
        so_5::send<BeginRetrospectiveTurn>(*this);
        PublishStateChange();
    }

    // if the agent queues two actions in one turn its an error!
    void AssertNextActionUnset() const {
        if (std::holds_alternative<std::monostate>(handle_this_turn_)) {
            return;
        }
        const bool is_white = so_is_active_state(white_turn) || so_is_active_state(white_action);
        throw std::runtime_error{
            is_white
                ? "White player tried to queue more than one action on its turn"
                : "Black player tried to queue more than one action on its turn"
        };
    }

    [[nodiscard]]
    std::shared_ptr<IPlayer> GetPlayerForThisTurn() const {
        if (so_is_active_state(white_turn) || so_is_active_state(white_action) || so_is_active_state(white_retrospective)) {
            return state_.players.white;
        }
        if (so_is_active_state(black_turn) || so_is_active_state(black_action) || so_is_active_state(black_retrospective)) {
            return state_.players.black;
        }
        throw std::runtime_error{"GetPlayerForThisTurn() is not supported for this state"};
    }

};

} // namespace chess::game::execution
