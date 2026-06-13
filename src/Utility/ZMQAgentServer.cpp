#include "ZMQAgentServer.h"
#include <iostream>

ZMQAgentServer::ZMQAgentServer(const std::string& endpoint, const std::string& color)
    : m_Endpoint(endpoint), m_Color(color), m_LogTag("[GameOrchestrator(" + color + ")]"),
      m_Trajectory(std::make_shared<AgentTrajectory>()),
      m_ReceiveQueue(std::make_shared<moodycamel::ConcurrentQueue<std::string>>()) {

    ZMQPairChannelOptions options;
    options.endpoint = endpoint;
    m_Channel = std::make_shared<ZMQPairChannel>(options);
    m_Channel->SetReceiveQueue(m_ReceiveQueue);
}

ZMQAgentServer::~ZMQAgentServer() {
    Stop();
}

void ZMQAgentServer::Start() {
    m_Channel->Start();
    m_IsConnected = true;
    m_Trajectory->SetState(AgentState::Idle);
    std::cout << "[ZMQAgentServer] Channel started for " << m_Endpoint << " (" << m_Color << ")\n";
}

void ZMQAgentServer::Stop() {
    if (m_IsConnected) {
        m_Channel->Stop();
        m_IsConnected = false;
    }
}

void ZMQAgentServer::Update() {
    std::string msgStr;
    while (m_ReceiveQueue->try_dequeue(msgStr)) {
        try {
            auto j = nlohmann::json::parse(msgStr);
            // std::cout << m_LogTag << " Received Message: " << j.value("type", "unknown") << "\n";
            HandleMessage(j);
        } catch (const std::exception& e) {
            std::cerr << m_LogTag << " JSON Parse Error: " << e.what() << "\n";
        }
    }
}

void ZMQAgentServer::HandleMessage(const nlohmann::json& j) {
    using namespace chess::agent;
    std::string type = j.value("type", "unknown");

    if (type == message::Pong::kTypeTag) {
        m_PongReceived = true;
        m_Trajectory->AddEvent(InfoEvent{"Received PONG"});
    } else if (type == message::SetupResponse::kTypeTag) {
        m_SetupAckReceived = true;
        auto m = j.get<message::SetupResponse>();
        m_Trajectory->UpdateModelName(m.model);
        m_Trajectory->AddEvent(InfoEvent{"Agent ready (" + m.color + ") - " + m.model});
    } else if (type == message::ReasoningSnapshot::kTypeTag) {
        auto m = j.get<message::ReasoningSnapshot>();
        m_Trajectory->SetState(AgentState::Thinking);
        m_Trajectory->UpdateEvent(m.id, std::move(m));
    } else if (type == message::ResponseSnapshot::kTypeTag) {
        auto m = j.get<message::ResponseSnapshot>();
        m_Trajectory->UpdateEvent(m.id, std::move(m));
    } else if (type == message::QuipResponse::kTypeTag) {
        auto m = j.get<message::QuipResponse>();
        m_Trajectory->UpdateEvent(m.id, m);
        if (m_Quips) {
            m_Quips->enqueue(m.message);
        }
    } else if (type == message::GetBoardStateRequest::kTypeTag) {
        auto m = j.get<message::GetBoardStateRequest>();
        std::lock_guard<std::mutex> lock(m_BoardStateRequestsMtx);
        m_BoardStateRequests.push(std::move(m));
    } else if (type == message::EndTurnRequest::kTypeTag) {
        auto m = j.get<message::EndTurnRequest>();
        std::lock_guard<std::mutex> lock(m_EndTurnRequestsMtx);
        m_EndTurnRequests.push(std::move(m));
    } else if (type == message::MoveResponse::kTypeTag) {
        m_Trajectory->SetState(AgentState::Idle);
        auto m = j.get<message::MoveResponse>();
        m_Trajectory->AddEvent(MoveEvent{m});
        
        std::lock_guard<std::mutex> lock(m_MoveDecisionsMtx);
        m_MoveDecisions.push(m.algebraic_move_string);
    } else if (type == message::ErrorResponse::kTypeTag) {
        m_Trajectory->SetState(AgentState::Error);
        auto m = j.get<message::ErrorResponse>();
        m_Trajectory->AddEvent(m);

        std::lock_guard<std::mutex> lock(m_ErrorsMtx);
        m_Errors.push(m.message);
    }
}

std::optional<std::string> ZMQAgentServer::PopMoveDecision() {
    std::lock_guard<std::mutex> lock(m_MoveDecisionsMtx);
    if (m_MoveDecisions.empty()) return std::nullopt;
    std::string move = m_MoveDecisions.front();
    m_MoveDecisions.pop();
    return move;
}

std::optional<std::string> ZMQAgentServer::PopError() {
    std::lock_guard<std::mutex> lock(m_ErrorsMtx);
    if (m_Errors.empty()) return std::nullopt;
    std::string err = m_Errors.front();
    m_Errors.pop();
    return err;
}

std::optional<chess::agent::message::GetBoardStateRequest> ZMQAgentServer::PopBoardStateRequest() {
    std::lock_guard<std::mutex> lock(m_BoardStateRequestsMtx);
    if (m_BoardStateRequests.empty()) return std::nullopt;
    auto req = m_BoardStateRequests.front();
    m_BoardStateRequests.pop();
    return req;
}

std::optional<chess::agent::message::EndTurnRequest> ZMQAgentServer::PopEndTurn() {
    std::lock_guard<std::mutex> lock(m_EndTurnRequestsMtx);
    if (m_EndTurnRequests.empty()) return std::nullopt;
    auto req = m_EndTurnRequests.front();
    m_EndTurnRequests.pop();
    return req;
}

void ZMQAgentServer::Send(const chess::agent::message::Ping& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::SetupRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::StartMoveRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::GameHistory& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::ErrorRecoveryRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::GameEnd& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::RetrospectiveRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::Send(const chess::agent::message::GetBoardStateResponse& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}
