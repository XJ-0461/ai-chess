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

class ZMQAgentServer {
public:
    ZMQAgentServer(const std::string& endpoint, const std::string& color);
    ~ZMQAgentServer();

    void Start();
    void Stop();
    void Update(); // Called periodically to process incoming messages

    void SendPing();
    void SendSetup(const std::string& color);
    void SendMoveRequest();
    void SendGameHistory(const std::vector<std::string>& history);
    void SendErrorRecovery(const std::vector<std::string>& history, const std::vector<std::string>& errors);
    void SendEndGame(const std::string& winner, const std::string& cause);

    std::shared_ptr<AgentTrajectory> GetTrajectory() const { return m_Trajectory; }
    
    std::optional<std::string> PopMoveDecision();
    std::optional<std::string> PopError();
    bool IsSetupAckReceived() const { return m_SetupAckReceived; }
    bool IsPongReceived() const { return m_PongReceived; }
    void ResetHandshake() { m_PongReceived = false; m_SetupAckReceived = false; }

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
};
