#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <so_5/all.hpp>
#include <nlohmann/json.hpp>
#include <oneapi/tbb/concurrent_vector.h>

#include "IPlayer.hpp"
#include "AgentConnection.hpp"
#include "Chess/Board.h"
#include "Chess/Move.h"
#include "Utility/Random.hpp"
#include "Utility/AgentTrajectory.h"
#include "Application/AgentChat/Message/Handshake.hpp"
#include "Application/AgentChat/Message/GameFlow.hpp"
#include "Application/AgentChat/Message/Utility.hpp"
#include "Application/AgentChat/Message/BoardState.hpp"
#include "Application/AgentChat/Message/Usage.hpp"
#include "Game/Execution/Message/PlayerAction.hpp"
#include "Utility/Log/Switch.hpp"

namespace chess::game::player {

struct RemoteAgentPlayerConfiguration {
    std::string zmq_endpoint{};
    std::string color{};      // "WHITE" / "BLACK"
    std::string provider{};   // e.g. "OpenRouter"
    std::string model{};      // model id
    std::string api_key{};    // resolved provider credential
    bool enable_quip{false};
    bool enable_draw_offer{false};
    bool enable_resignation{false};
};

// Drives a single chess-agent over ZMQ. Each turn ("request") spawns a dedicated
// thread that sends the request, streams the agent's events for the duration of
// that request, routes them (UI events -> trajectory; the terminal game-flow
// result -> the orchestrator's mbox), then terminates. Requests are strictly
// sequential (the orchestrator only asks for one thing at a time).
class RemoteAgentPlayer : public IPlayer {
public:
    RemoteAgentPlayer(
        RemoteAgentPlayerConfiguration config,
        Colour colour,
        so_5::mbox_t orchestrator_mbox,
        std::shared_ptr<AgentTrajectory> trajectory,
        std::shared_ptr<Board> board,
        std::shared_ptr<std::mutex> board_mutex,
        std::shared_ptr<tbb::concurrent_vector<agent::message::ModelUsage>> usage_history,
        std::shared_ptr<agent::message::ModelPricing> model_pricing
    ) : config_{std::move(config)},
        colour_{colour},
        orchestrator_mbox_{std::move(orchestrator_mbox)},
        trajectory_{std::move(trajectory)},
        board_{std::move(board)},
        board_mutex_{std::move(board_mutex)},
        usage_history_{std::move(usage_history)},
        model_pricing_{std::move(model_pricing)},
        connection_{config_.zmq_endpoint} {}

    // Blocking: opens the socket and performs the handshake. Intended to run on
    // a setup BlockingTask thread.
    void Connect() {
        namespace msg = agent::message;

        if (!connection_.Open()) {
            trajectory_->AddEvent(chess::agent::InfoEvent{"Could not connect to agent at " + config_.zmq_endpoint, true});
            trajectory_->SetState(AgentState::Error);
            return;
        }

        connection_.Send(nlohmann::json(msg::Ping{}));
        const auto pong = WaitForType(msg::Pong::kTypeTag, std::chrono::seconds(5));
        if (!pong) {
            trajectory_->AddEvent(agent::InfoEvent{"Agent setup timed out", true});
            trajectory_->SetState(AgentState::Error);
        }

        msg::SetupRequest setup;
        setup.color = config_.color;
        setup.provider = config_.provider;
        setup.model = config_.model;
        setup.api_key = config_.api_key;
        setup.enable_quip = config_.enable_quip;
        setup.enable_draw_offer = config_.enable_draw_offer;
        setup.enable_resignation = config_.enable_resignation;
        connection_.Send(nlohmann::json(setup));

        if (const auto ack = WaitForType(msg::SetupResponse::kTypeTag, std::chrono::seconds(30))) {
            const auto response = ack->get<msg::SetupResponse>();
            personality_ = response.personality;
            if (model_pricing_) {
                *model_pricing_ = response.pricing;
            }
            trajectory_->UpdateModelName(response.model);
            trajectory_->AddEvent(agent::InfoEvent{"Agent ready: " + response.model + " [" + response.personality + "]"});
            trajectory_->SetState(AgentState::Idle);
        } else {
            trajectory_->AddEvent(agent::InfoEvent{"Agent setup timed out", true});
            trajectory_->SetState(AgentState::Error);
        }
    }

    void BeginTurn(const TurnContext& context) override {
        trajectory_->SetCurrentMoveCount(context.turn_number);
        StartRequest(BuildMoveRequest(context));
    }

    void BeginHandleDrawOffer(const TurnContext& context) override {
        trajectory_->SetCurrentMoveCount(context.turn_number);
        StartRequest(BuildDrawOfferRequest(context));
    }

    void BeginHandleOpponentDeclinedDrawOffer(const TurnContext& context) override {
        trajectory_->SetCurrentMoveCount(context.turn_number);
        StartRequest(BuildDrawDeclinedRequest(context));
    }

    void BeginErrorRecoveryTurn(const TurnContext& context) override {
        trajectory_->SetCurrentMoveCount(context.turn_number);
        StartRequest(BuildErrorRecoveryRequest(context));
    }

    void BeginRetrospectiveTurn(const TurnContext& context) override {
        trajectory_->SetCurrentMoveCount(context.turn_number);
        StartRequest(BuildRetrospectiveRequest(context));
    }

    [[nodiscard]] std::shared_ptr<AgentTrajectory> GetTrajectory() const { return trajectory_; }

    // Session cost is computed from the shared usage/pricing slots (which are also
    // reachable via GameContext, so they survive this player); convenience for
    // callers holding the player. The command path computes the same way.
    [[nodiscard]]
    agent::message::SessionCostEstimate EstimateSessionCost() const {
        if (!model_pricing_ || !usage_history_) {
            return {};
        }
        return agent::message::EstimateSessionCost(*model_pricing_, *usage_history_);
    }

private:
    static constexpr auto kRequestTimeout = std::chrono::seconds(180);
    static constexpr auto kReceivePollInterval = std::chrono::milliseconds(200);

    // --- Request builders ---

    [[nodiscard]] nlohmann::json BuildMoveRequest(const TurnContext& context) const {
        chess::agent::message::StartMoveRequest request;
        request.game_history = context.game_history;
        request.opponent_quips = context.opponent_quips;
        request.own_quip_history = context.own_quip_history;
        request.personality = personality_;
        request.turn_number = context.turn_number;
        return request;
    }

    [[nodiscard]] nlohmann::json BuildDrawOfferRequest(const TurnContext& context) const {
        chess::agent::message::DrawOfferRequest request;
        request.game_history = context.game_history;
        request.opponent_quips = context.opponent_quips;
        request.own_quip_history = context.own_quip_history;
        request.personality = personality_;
        return request;
    }

    [[nodiscard]] nlohmann::json BuildDrawDeclinedRequest(const TurnContext& context) const {
        chess::agent::message::DrawOfferDeclinedRequest request;
        request.game_history = context.game_history;
        request.opponent_quips = context.opponent_quips;
        request.own_quip_history = context.own_quip_history;
        request.personality = personality_;
        return request;
    }

    [[nodiscard]] nlohmann::json BuildErrorRecoveryRequest(const TurnContext& context) const {
        chess::agent::message::ErrorRecoveryRequest request;
        request.game_history = context.game_history;
        request.opponent_quips = context.opponent_quips;
        request.own_quip_history = context.own_quip_history;
        request.errors = context.errors;
        request.personality = personality_;
        return request;
    }

    [[nodiscard]] nlohmann::json BuildRetrospectiveRequest(const TurnContext& context) const {
        chess::agent::message::RetrospectiveRequest request;
        request.game_history = context.game_history;
        request.opponent_quips = context.opponent_quips;
        request.own_quip_history = context.own_quip_history;
        request.winner = context.winner;
        request.cause = context.cause;
        request.personality = personality_;
        return request;
    }

    // Spawns the dedicated request-handler thread. Reassigning the jthread joins
    // the previous (already-finished) request.
    void StartRequest(nlohmann::json request) {
        request_thread_ = std::jthread([this, request = std::move(request)](std::stop_token stop_token) {
            if (!connection_.Send(request)) {
                ReportFailure("Failed to send request to agent");
                return;
            }

            const auto deadline = std::chrono::steady_clock::now() + kRequestTimeout;
            while (!stop_token.stop_requested()) {
                if (std::chrono::steady_clock::now() > deadline) {
                    ReportFailure("Agent request timed out");
                    return;
                }
                const auto event = connection_.Receive(kReceivePollInterval);
                if (!event) {
                    continue;
                }
                if (RouteEvent(*event)) {
                    return; // terminal event reached; request complete
                }
            }
            // Stopped (player being torn down): nothing to report.
        });
    }

    // Records a failure in the trajectory AND notifies the orchestrator so it
    // can recover/forfeit instead of waiting forever for an action.
    void ReportFailure(const std::string& message) {
        trajectory_->AddEvent(chess::agent::InfoEvent{message, true});
        trajectory_->SetState(AgentState::Error);
        so_5::send<execution::PlayerError>(orchestrator_mbox_, colour_, message);
    }

    // Blocking receive until a message of `type` arrives or the timeout elapses.
    [[nodiscard]] std::optional<nlohmann::json> WaitForType(const std::string& type, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto message = connection_.Receive(kReceivePollInterval)) {
                if (message->value("type", std::string{}) == type) {
                    return message;
                }
            }
        }
        return std::nullopt;
    }

    // Routes one streamed event. Returns true when the event terminates the
    // request (a move/draw/resign decision, a retrospective end, or an error).
    bool RouteEvent(const nlohmann::json& event) {
        namespace msg = chess::agent::message;
        const std::string type = event.value("type", std::string{});

        // --- UI-only streaming events (not crucial for game flow) ---
        if (type == msg::ReasoningSnapshot::kTypeTag) {
            auto m = event.get<msg::ReasoningSnapshot>();
            trajectory_->SetState(AgentState::Thinking);
            trajectory_->UpdateEvent(m.id, std::move(m));
            return false;
        }
        if (type == msg::ResponseSnapshot::kTypeTag) {
            auto m = event.get<msg::ResponseSnapshot>();
            trajectory_->UpdateEvent(m.id, std::move(m));
            return false;
        }
        if (type == msg::QuipResponse::kTypeTag) {
            auto m = event.get<msg::QuipResponse>();
            CHESS_TRACE_LOG("stdout_chess",
                {"operation", "quip_received"},
                {"colour", colour_ == White ? "white" : "black"},
                {"quip_id", m.id},
                {"trajectory_ptr", std::to_string(reinterpret_cast<uintptr_t>(trajectory_.get()))}
            );
            trajectory_->UpdateEvent(m.id, m);
            so_5::send<execution::PlayerQuip>(orchestrator_mbox_, execution::PlayerQuip{m.message});
            return false;
        }
        if (type == msg::GetBoardStateRequest::kTypeTag) {
            RespondBoardState(event.value("id", std::string{}));
            return false;
        }
        if (type == msg::ModelUsage::kTypeTag) {
            // Per-turn token usage; accumulated for session cost estimation. Sent
            // before the terminal decision, so it lands in this same loop. The
            // concurrent_vector is safe to append while the command thread reads.
            const auto usage = event.get<msg::ModelUsage>();
            CHESS_TRACE_LOG("stdout_chess",
                {"operation", "model_usage_received"},
                {"colour", colour_ == White ? "white" : "black"},
                {"input_tokens", std::to_string(usage.input_tokens)},
                {"output_tokens", std::to_string(usage.output_tokens)},
                {"usage_history_attached", usage_history_ ? "true" : "false"}
            );
            if (usage_history_) {
                usage_history_->push_back(usage);
            }
            return false;
        }

        // --- Terminal game-flow results (routed to the orchestrator) ---
        if (type == msg::MoveResponse::kTypeTag) {
            auto m = event.get<msg::MoveResponse>();
            trajectory_->SetState(AgentState::Idle);
            // One id correlates the trajectory entry, the sidebar bubble and the
            // GameMoveLog outcome for this move attempt.
            const std::string move_id = chess::util::RandomAlphaString(8);
            trajectory_->AddEvent(chess::agent::MoveEvent{m}, move_id);
            so_5::send<execution::PlayerMoveAction>(orchestrator_mbox_, colour_, m.long_algebraic_move_string, move_id);
            so_5::send<execution::PlayerYield>(orchestrator_mbox_);
            return true;
        }
        if (type == msg::OfferDrawResponse::kTypeTag) {
            trajectory_->SetState(AgentState::Idle);
            trajectory_->AddEvent(chess::agent::InfoEvent{"Agent offered a draw"});
            so_5::send<execution::PlayerOfferDrawAction>(orchestrator_mbox_, colour_);
            so_5::send<execution::PlayerYield>(orchestrator_mbox_);
            return true;
        }
        if (type == msg::ResignResponse::kTypeTag) {
            trajectory_->SetState(AgentState::Idle);
            trajectory_->AddEvent(chess::agent::InfoEvent{"Agent resigned"});
            so_5::send<execution::PlayerSubmitResignationAction>(orchestrator_mbox_, colour_);
            so_5::send<execution::PlayerYield>(orchestrator_mbox_);
            return true;
        }
        if (type == msg::DrawDecisionResponse::kTypeTag) {
            const auto m = event.get<msg::DrawDecisionResponse>();
            trajectory_->AddEvent(chess::agent::InfoEvent{m.accept ? "Agent accepted the draw" : "Agent declined the draw"});
            if (m.accept) {
                so_5::send<execution::PlayerAcceptDrawAction>(orchestrator_mbox_, colour_);
            } else {
                so_5::send<execution::PlayerDeclineDrawAction>(orchestrator_mbox_, colour_);
            }
            so_5::send<execution::PlayerYield>(orchestrator_mbox_);
            return true;
        }
        if (type == msg::EndTurnRequest::kTypeTag) {
            // Retrospective finished; notify the orchestrator so it can
            // transition to the next retrospective turn or conclude the game.
            trajectory_->SetState(AgentState::Idle);
            so_5::send<execution::PlayerYield>(orchestrator_mbox_);
            return true;
        }
        if (type == msg::ErrorResponse::kTypeTag) {
            const auto m = event.get<msg::ErrorResponse>();
            trajectory_->SetState(AgentState::Error);
            trajectory_->AddEvent(m);
            // Tell the orchestrator the turn failed so it can recover/forfeit
            // (otherwise it waits forever for an action that never comes).
            so_5::send<execution::PlayerError>(orchestrator_mbox_, colour_, m.message);
            return true;
        }

        return false; // unknown / handshake-only message mid-request
    }

    void RespondBoardState(const std::string& id) {
        std::string fen;
        nlohmann::json board_matrix = nlohmann::json::object();

        if (board_ && board_mutex_) {
            const std::lock_guard<std::mutex> lock(*board_mutex_);
            fen = board_->ToFEN();

            // Build board matrix organized by rank (1-8), each with files a-h
            for (int rank = 0; rank < 8; ++rank) {
                nlohmann::json files = nlohmann::json::array();
                for (int file = 0; file < 8; ++file) {
                    const Square square = static_cast<Square>(rank * 8 + file);
                    const Piece piece = (*board_)[square];

                    std::string piece_str;
                    if (piece != Piece::None) {
                        const Colour color = GetColour(piece);
                        const PieceType type = GetPieceType(piece);

                        // Format: (W|B)(P|R|N|B|Q|K)
                        piece_str += (color == White) ? 'W' : 'B';
                        piece_str += PieceTypeToChar(type);
                    }
                    // Empty squares get empty string ""
                    files.push_back(piece_str);
                }

                // Store with explicit rank number (1-8, not 0-7)
                const std::string rank_key = std::to_string(rank + 1);
                board_matrix[rank_key] = nlohmann::json{{"files_a_to_h", files}};
            }
        }

        chess::agent::message::GetBoardStateResponse response;
        response.id = id;
        response.data = nlohmann::json{
            {"fen", fen},
            {"board_matrix", board_matrix}
        };
        connection_.Send(nlohmann::json(response));
    }

    RemoteAgentPlayerConfiguration config_;
    Colour colour_;
    so_5::mbox_t orchestrator_mbox_;
    std::shared_ptr<AgentTrajectory> trajectory_;
    std::shared_ptr<Board> board_;
    std::shared_ptr<std::mutex> board_mutex_;
    std::string personality_{};

    // Shared with GameContext: this player appends per-turn usage here and writes
    // pricing once at setup; the game::estimate_cost command reads them.
    std::shared_ptr<tbb::concurrent_vector<agent::message::ModelUsage>> usage_history_;
    std::shared_ptr<agent::message::ModelPricing> model_pricing_;

    AgentConnection connection_;
    std::jthread request_thread_; // declared last: joined first on destruction
};

} // namespace chess::game::player
