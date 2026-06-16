#include "ZMQAgentClient.h"
#include <iostream>

namespace chess::agent {

ZMQAgentClient::ZMQAgentClient(
    const std::string& endpoint,
    const std::string& color
    ) : m_Endpoint(endpoint),
        m_Color(color),
        m_LogTag("[ZMQAgentClient(" + color + ")]"),
        m_Trajectory(std::make_shared<AgentTrajectory>()),
        m_ReceiveQueue(std::make_shared<moodycamel::ConcurrentQueue<std::string>>()) {

    ZMQPairChannelOptions options;
    options.endpoint = endpoint;
    m_Channel = std::make_shared<ZMQPairChannel>(options);
    m_Channel->SetReceiveQueue(m_ReceiveQueue);
}

ZMQAgentClient::~ZMQAgentClient() {
    Stop();
}

void ZMQAgentClient::Start() {
    m_Channel->Start();
    m_IsConnected = true;
    m_Trajectory->SetState(AgentState::Idle);
    std::cout << "[ZMQAgentServer] Channel started for " << m_Endpoint << " (" << m_Color << ")\n";
}

void ZMQAgentClient::Stop() {
    if (m_IsConnected) {
        m_Channel->Stop();
        m_IsConnected = false;
    }
}

void ZMQAgentClient::Update() {
    std::string message{};
    while (m_ReceiveQueue->try_dequeue(message)) {
        try {
            HandleMessage(nlohmann::json::parse(message));
        } catch (const std::exception& ex) {
            std::cerr << m_LogTag << " JSON Parse Error: " << ex.what() << "\n";
        }
    }
}

void ZMQAgentClient::HandleMessage(const nlohmann::json& j) {
    const std::string type = j.value("type", "unknown");

    if (type == message::Pong::kTypeTag) {
        m_PongReceived = true;
        m_Trajectory->AddEvent(InfoEvent{"Received PONG"});
    } else if (type == message::SetupResponse::kTypeTag) {
        m_SetupAckReceived = true;
        auto m = j.get<message::SetupResponse>();
        m_Personality = m.personality;
        m_Trajectory->UpdateModelName(m.model);
        m_Trajectory->AddEvent(InfoEvent{"Agent ready (" + m.color + ") - " + m.model + " [" + m_Personality + "]"});
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
    } else if (type == message::ResignResponse::kTypeTag) {
        m_Trajectory->SetState(AgentState::Idle);
        m_Trajectory->AddEvent(InfoEvent{"Agent resigned"});
        std::lock_guard<std::mutex> lock(m_ResignationsMtx);
        m_Resignations.push(true);
    } else if (type == message::OfferDrawResponse::kTypeTag) {
        m_Trajectory->SetState(AgentState::Idle);
        m_Trajectory->AddEvent(InfoEvent{"Agent offered a draw"});
        std::lock_guard<std::mutex> lock(m_DrawOffersMtx);
        m_DrawOffers.push(true);
    } else if (type == message::DrawDecisionResponse::kTypeTag) {
        auto m = j.get<message::DrawDecisionResponse>();
        m_Trajectory->AddEvent(InfoEvent{m.accept ? "Agent accepted draw offer" : "Agent declined draw offer"});
        std::lock_guard<std::mutex> lock(m_DrawDecisionsMtx);
        m_DrawDecisions.push(m.accept);
    } else if (type == message::ErrorResponse::kTypeTag) {
        m_Trajectory->SetState(AgentState::Error);
        auto m = j.get<message::ErrorResponse>();
        m_Trajectory->AddEvent(m);
        std::lock_guard<std::mutex> lock(m_ErrorsMtx);
        m_Errors.push(m.message);
    }
}

std::optional<std::string> ZMQAgentClient::PopMoveDecision() {
    std::lock_guard<std::mutex> lock(m_MoveDecisionsMtx);
    if (m_MoveDecisions.empty()) return std::nullopt;
    std::string move = m_MoveDecisions.front();
    m_MoveDecisions.pop();
    return move;
}

std::optional<bool> ZMQAgentClient::PopResign() {
    std::lock_guard<std::mutex> lock(m_ResignationsMtx);
    if (m_Resignations.empty()) return std::nullopt;
    bool res = m_Resignations.front();
    m_Resignations.pop();
    return res;
}

std::optional<bool> ZMQAgentClient::PopOfferDraw() {
    std::lock_guard<std::mutex> lock(m_DrawOffersMtx);
    if (m_DrawOffers.empty()) return std::nullopt;
    bool res = m_DrawOffers.front();
    m_DrawOffers.pop();
    return res;
}

std::optional<bool> ZMQAgentClient::PopDrawDecision() {
    std::lock_guard<std::mutex> lock(m_DrawDecisionsMtx);
    if (m_DrawDecisions.empty()) return std::nullopt;
    bool res = m_DrawDecisions.front();
    m_DrawDecisions.pop();
    return res;
}

std::optional<std::string> ZMQAgentClient::PopError() {
    std::lock_guard<std::mutex> lock(m_ErrorsMtx);
    if (m_Errors.empty()) return std::nullopt;
    std::string err = m_Errors.front();
    m_Errors.pop();
    return err;
}

std::optional<message::GetBoardStateRequest> ZMQAgentClient::PopBoardStateRequest() {
    std::lock_guard<std::mutex> lock(m_BoardStateRequestsMtx);
    if (m_BoardStateRequests.empty()) return std::nullopt;
    auto req = m_BoardStateRequests.front();
    m_BoardStateRequests.pop();
    return req;
}

std::optional<message::EndTurnRequest> ZMQAgentClient::PopEndTurn() {
    std::lock_guard<std::mutex> lock(m_EndTurnRequestsMtx);
    if (m_EndTurnRequests.empty()) return std::nullopt;
    auto req = m_EndTurnRequests.front();
    m_EndTurnRequests.pop();
    return req;
}

void ZMQAgentClient::Send(const message::Ping& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::SetupRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::StartMoveRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::GameHistory& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::ErrorRecoveryRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::GameEnd& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::RetrospectiveRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::DrawOfferRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::DrawOfferDeclinedRequest& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentClient::Send(const message::GetBoardStateResponse& m) {
    nlohmann::json j = m;
    m_Channel->SendMessage(j.dump());
}

} // namespace chess::agent