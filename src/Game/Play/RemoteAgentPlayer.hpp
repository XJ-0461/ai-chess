#pragma once

#include <future>
#include <memory>
#include <string>
#include <variant>

#include "IPlayer.hpp"
#include "Application/AgentChat/Message/GameFlow.hpp"
#include "Application/AgentChat/Message/Utility.hpp"
#include "Utility/ZMQAgentClient.h"

namespace chess::game::player {

using AgentTrajectoryEvent = std::variant<
    agent::message::ModelReasoningSnapshot,
    agent::message::ModelResponseSnapshot,
    agent::message::QuipResponse,
    agent::message::MoveResponse,
    agent::message::ErrorResponse
>;

struct AgentTrajectory {
    std::vector<AgentTrajectoryEvent> history{};
};

struct RemoteAgentPlayerConfiguration {
    std::string zmq_endpoint{};
    bool enable_quip{false};
    bool enable_draw_offer{false};
    bool enable_resignation{false};
};

class RemoteAgentPlayer : public IPlayer {
public:

    explicit RemoteAgentPlayer(
        const RemoteAgentPlayerConfiguration& config
    ): config_{std::move(config)} {}

    void Connect() {

    }

    void BeginTurn() override {

    }

    void BeginHandleDrawOffer() override {

    }

    void BeginHandleOpponentDeclinedDrawOffer() override {

    }

    void BeginErrorRecoveryTurn() override {

    }

    void BeginRetrospectiveTurn() override {

    }

private:

    RemoteAgentPlayerConfiguration config_{};
    std::shared_ptr<agent::ZMQAgentClient> remote_agent_{};

};

} // namespace chess::game::player
