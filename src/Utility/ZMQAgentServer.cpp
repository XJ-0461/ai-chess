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
            std::cout << m_LogTag << " Received Message: " << j.value("type", "unknown") << "\n";
            HandleMessage(j);
        } catch (const std::exception& e) {
            std::cerr << m_LogTag << " JSON Parse Error: " << e.what() << "\n";
        }
    }
}

void ZMQAgentServer::HandleMessage(const nlohmann::json& j) {
    std::string type = j.value("type", "unknown");
    std::string id = j.value("id", "");

    if (type == "pong") {
        m_PongReceived = true;
        m_Trajectory->AddEvent("info", "Received PONG");
    } else if (type == "setup_ack") {
        m_SetupAckReceived = true;
        m_Trajectory->modelName = j.value("model", "Unknown Model");
        m_Trajectory->AddEvent("info", "Agent ready (" + j.value("color", "unknown") + ") - " + m_Trajectory->modelName);
    } else if (type == "reasoning") {
        m_Trajectory->SetState(AgentState::Thinking);
        m_Trajectory->UpdateEvent(id, "reasoning", j.value("message", ""));
    } else if (type == "response") {
        m_Trajectory->UpdateEvent(id, "response", j.value("message", ""));
    } else if (type == "quip") {
        m_Trajectory->UpdateEvent(id, "quip", j.value("message", ""));
        if (m_Quips) {
            m_Quips->enqueue(j.value("message", ""));
        }
    } else if (type == "fetch_board_state") {
        std::lock_guard<std::mutex> lock(m_BoardStateRequestsMtx);
        m_BoardStateRequests.push({id});
    } else if (type == "end_turn") {
        std::lock_guard<std::mutex> lock(m_EndTurnRequestsMtx);
        m_EndTurnRequests.push({id});
    } else if (type == "move_decision") {
        m_Trajectory->SetState(AgentState::Idle);
        std::string move = j.value("algebraic_move_string", "");
        m_Trajectory->AddEvent("move", move);
        std::lock_guard<std::mutex> lock(m_MoveDecisionsMtx);
        m_MoveDecisions.push(move);
    } else if (type == "ERROR") {
        m_Trajectory->SetState(AgentState::Error);
        std::string msg = j.value("message", "Unknown error");
        m_Trajectory->AddEvent("error", msg);

        std::lock_guard<std::mutex> lock(m_ErrorsMtx);
        m_Errors.push(msg);
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

std::optional<ZMQAgentServer::BoardStateRequest> ZMQAgentServer::PopBoardStateRequest() {
    std::lock_guard<std::mutex> lock(m_BoardStateRequestsMtx);
    if (m_BoardStateRequests.empty()) return std::nullopt;
    auto req = m_BoardStateRequests.front();
    m_BoardStateRequests.pop();
    return req;
}

std::optional<ZMQAgentServer::EndTurnRequest> ZMQAgentServer::PopEndTurn() {
    std::lock_guard<std::mutex> lock(m_EndTurnRequestsMtx);
    if (m_EndTurnRequests.empty()) return std::nullopt;
    auto req = m_EndTurnRequests.front();
    m_EndTurnRequests.pop();
    return req;
}

void ZMQAgentServer::SendBoardStateResponse(const std::string& id, const nlohmann::json& data) {
    nlohmann::json j = data;
    j["type"] = "board_state_response";
    j["id"] = id;
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendPing() {
    std::cout << m_LogTag << " Sending Message: ping\n";
    nlohmann::json j = {{"type", "ping"}};
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendSetup(const std::string& color) {
    std::cout << m_LogTag << " Sending Message: setup\n";
    nlohmann::json j = {{"type", "setup"}, {"color", color}};
    m_Channel->SendMessage(j.dump());
}

// refactor the "start_move" request to include the move history AND any potential quips sent by the opponent.
void ZMQAgentServer::SendMoveRequest(
    const std::vector<std::string>& history,
    const std::vector<std::string>& opponent_quips
) {
    std::cout << m_LogTag << " Sending Message: start_move\n";
    nlohmann::json j = {
        {"type", "start_move"},
        {"game_history", history},
        {"opponent_quips", opponent_quips}
    };
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendGameHistory(const std::vector<std::string>& history) {
    std::cout << m_LogTag << " Sending Message: game_history\n";
    nlohmann::json j = {{"type", "game_history"}, {"game_history", history}};
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendErrorRecovery(const std::vector<std::string>& history, const std::vector<std::string>& errors) {
    std::cout << m_LogTag << " Sending Message: error_recovery\n";
    nlohmann::json j = {{"type", "error_recovery"}, {"game_history", history}, {"errors", errors}};
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendEndGame(const std::string& winner, const std::string& cause) {
    std::cout << m_LogTag << " Sending Message: end_game\n";
    nlohmann::json j = {{"type", "end_game"}, {"winner", winner}, {"cause", cause}};
    m_Channel->SendMessage(j.dump());
}

void ZMQAgentServer::SendRetrospectiveRequest(const std::vector<std::string>& history, const std::vector<std::string>& opponent_quips, const std::string& winner, const std::string& cause) {
    std::cout << m_LogTag << " Sending Message: retrospective_request\n";
    nlohmann::json j = {
        {"type", "retrospective_request"},
        {"game_history", history},
        {"opponent_quips", opponent_quips},
        {"winner", winner},
        {"cause", cause}
    };
    m_Channel->SendMessage(j.dump());
}
