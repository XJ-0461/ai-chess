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

class ZMQAgentServer {
public:
    ZMQAgentServer(const std::string& endpoint, const std::string& color);
    ~ZMQAgentServer();

    void Start();
    void Stop();
    void Update(); // Called periodically to process incoming messages

    void Send(const chess::agent::message::Ping& m);
    void Send(const chess::agent::message::SetupRequest& m);
    void Send(const chess::agent::message::StartMoveRequest& m);
    void Send(const chess::agent::message::GameHistory& m);
    void Send(const chess::agent::message::ErrorRecoveryRequest& m);
    void Send(const chess::agent::message::GameEnd& m);
    void Send(const chess::agent::message::RetrospectiveRequest& m);
    void Send(const chess::agent::message::GetBoardStateResponse& m);

    std::shared_ptr<AgentTrajectory> GetTrajectory() const { return m_Trajectory; }
    
    std::optional<std::string> PopMoveDecision();
    std::optional<std::string> PopError();

    std::optional<chess::agent::message::EndTurnRequest> PopEndTurn();
    std::optional<chess::agent::message::GetBoardStateRequest> PopBoardStateRequest();

    bool IsSetupAckReceived() const { return m_SetupAckReceived; }
    bool IsPongReceived() const { return m_PongReceived; }
    void ResetHandshake() { m_PongReceived = false; m_SetupAckReceived = false; }

    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> GetQuipsQueue() { return m_Quips; }

private:
    void HandleMessage(const nlohmann::json& j);

    std::string m_Endpoint;
    std::string m_Color;
    std::string m_LogTag;
    std::shared_ptr<AgentTrajectory> m_Trajectory;

    std::shared_ptr<ZMQPairChannel> m_Channel;
    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> m_ReceiveQueue{};

    std::atomic<bool> m_IsConnected{ false };
    std::atomic<bool> m_SetupAckReceived{ false };
    std::atomic<bool> m_PongReceived{ false };

    std::queue<std::string> m_MoveDecisions;
    std::mutex m_MoveDecisionsMtx;

    std::queue<std::string> m_Errors;
    std::mutex m_ErrorsMtx;

    std::queue<chess::agent::message::GetBoardStateRequest> m_BoardStateRequests;
    std::mutex m_BoardStateRequestsMtx;

    std::queue<chess::agent::message::EndTurnRequest> m_EndTurnRequests;
    std::mutex m_EndTurnRequestsMtx;

    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> m_Quips{std::make_shared<moodycamel::ConcurrentQueue<std::string>>()};
};
