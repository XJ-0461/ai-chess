#pragma once

#include "AgentTrajectory.h"
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <queue>
#include <optional>
#include <mutex>
#include "ZMQPairChannel.hpp"

#include "Application/AgentChat/Message/Handshake.hpp"
#include "Application/AgentChat/Message/GameFlow.hpp"
#include "Application/AgentChat/Message/Utility.hpp"
#include "Application/AgentChat/Message/BoardState.hpp"

namespace chess::agent {

class ZMQAgentClient {
public:
    ZMQAgentClient(const std::string& endpoint, const std::string& color);
    ~ZMQAgentClient();

    void Start();
    void Stop();
    void Update(); // Called periodically to process incoming messages

    void Send(const message::Ping& m);
    void Send(const message::SetupRequest& m);
    void Send(const message::StartMoveRequest& m);
    void Send(const message::GameHistory& m);
    void Send(const message::ErrorRecoveryRequest& m);
    void Send(const message::GameEnd& m);
    void Send(const message::RetrospectiveRequest& m);
    void Send(const message::DrawOfferRequest& m);
    void Send(const message::DrawOfferDeclinedRequest& m);
    void Send(const message::GetBoardStateResponse& m);

    std::shared_ptr<AgentTrajectory> GetTrajectory() const {
        return m_Trajectory;
    }

    std::optional<std::string> PopMoveDecision();
    std::optional<bool> PopResign();
    std::optional<bool> PopOfferDraw();
    std::optional<bool> PopDrawDecision();
    std::optional<std::string> PopError();

    std::optional<message::EndTurnRequest> PopEndTurn();
    std::optional<message::GetBoardStateRequest> PopBoardStateRequest();

    bool IsSetupAckReceived() const { return m_SetupAckReceived; }
    std::string GetPersonality() const { return m_Personality; }
    bool IsPongReceived() const { return m_PongReceived; }
    void ResetHandshake() { m_PongReceived = false; m_SetupAckReceived = false; m_Personality = ""; }

    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> GetQuipsQueue() { return m_Quips; }

private:
    void HandleMessage(const nlohmann::json& j);

    std::string m_Endpoint;
    std::string m_Color;
    std::string m_LogTag;
    std::string m_Personality;
    std::shared_ptr<AgentTrajectory> m_Trajectory;

    std::shared_ptr<ZMQPairChannel> m_Channel;
    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> m_ReceiveQueue{};

    std::atomic<bool> m_IsConnected{ false };
    std::atomic<bool> m_SetupAckReceived{ false };
    std::atomic<bool> m_PongReceived{ false };

    std::queue<std::string> m_MoveDecisions;
    std::mutex m_MoveDecisionsMtx;

    std::queue<bool> m_Resignations;
    std::mutex m_ResignationsMtx;

    std::queue<bool> m_DrawOffers;
    std::mutex m_DrawOffersMtx;

    std::queue<bool> m_DrawDecisions;
    std::mutex m_DrawDecisionsMtx;

    std::queue<std::string> m_Errors;
    std::mutex m_ErrorsMtx;

    std::queue<message::GetBoardStateRequest> m_BoardStateRequests;
    std::mutex m_BoardStateRequestsMtx;

    std::queue<message::EndTurnRequest> m_EndTurnRequests;
    std::mutex m_EndTurnRequestsMtx;

    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> m_Quips{std::make_shared<moodycamel::ConcurrentQueue<std::string>>()};
};

} // namespace chess::agent
