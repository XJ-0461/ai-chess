#pragma once

#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include "Chess/Board.h"
#include "ZMQAgentServer.h"

enum class MatchState {
    Handshake,
    PendingSetup,
    WhiteTurn,
    BlackTurn,
    GameOver
};

class GameOrchestrator {
public:
    GameOrchestrator(
        std::shared_ptr<ZMQAgentServer> whiteAgent,
        std::shared_ptr<ZMQAgentServer> blackAgent,
        std::shared_ptr<Board> board,
        std::shared_ptr<std::mutex> boardMutex
    );
    ~GameOrchestrator();

    void Start();
    void Stop();

    MatchState GetState() const { return m_State; }
    std::string GetCurrentFEN();

private:
    void OrchestratorLoop(std::stop_token st);
    void HandleTurn(std::shared_ptr<ZMQAgentServer> activeAgent, std::shared_ptr<ZMQAgentServer> opponentAgent, MatchState nextState);

    std::shared_ptr<ZMQAgentServer> m_WhiteAgent;
    std::shared_ptr<ZMQAgentServer> m_BlackAgent;
    std::shared_ptr<Board> m_Board;
    std::shared_ptr<std::mutex> m_BoardMutex;

    std::vector<std::string> m_GameHistory;
    MatchState m_State = MatchState::PendingSetup;
    std::jthread m_Thread;
};
