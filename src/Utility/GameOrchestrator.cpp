#include "GameOrchestrator.h"
#include "../Chess/ChessException.h"
#include <iostream>
#include <chrono>

GameOrchestrator::GameOrchestrator(
    std::shared_ptr<ZMQAgentServer> whiteAgent,
    std::shared_ptr<ZMQAgentServer> blackAgent,
    std::shared_ptr<Board> board,
    std::shared_ptr<std::mutex> boardMutex
) : m_WhiteAgent(whiteAgent), m_BlackAgent(blackAgent), m_Board(board), m_BoardMutex(boardMutex), m_State(MatchState::Handshake) {
}

GameOrchestrator::~GameOrchestrator() {
    Stop();
}

void GameOrchestrator::Start() {
    m_Thread = std::jthread([this]() {
        OrchestratorLoop(std::stop_token{});
    });
}

void GameOrchestrator::Stop() {
    m_Thread.request_stop();
}

std::string GameOrchestrator::GetCurrentFEN() {
    std::lock_guard<std::mutex> lock(*m_BoardMutex);
    return m_Board->ToFEN();
}

void GameOrchestrator::OrchestratorLoop(std::stop_token st) {
    std::cout << "[GameOrchestrator] Starting loop...\n";

    while (!st.stop_requested()) {
        if (m_WhiteAgent) m_WhiteAgent->Update();
        if (m_BlackAgent) m_BlackAgent->Update();

        switch (m_State) {
            case MatchState::Handshake:
                if (m_WhiteAgent && m_BlackAgent) {
                    static auto lastPing = std::chrono::steady_clock::now();
                    static bool first = true;
                    if (first || std::chrono::steady_clock::now() - lastPing > std::chrono::milliseconds(1000)) {
                        m_WhiteAgent->SendPing();
                        m_BlackAgent->SendPing();
                        lastPing = std::chrono::steady_clock::now();
                        first = false;
                    }
                    
                    if (m_WhiteAgent->IsPongReceived() && m_BlackAgent->IsPongReceived()) {
                        std::cout << "[GameOrchestrator] Handshake complete.\n";
                        m_State = MatchState::PendingSetup;
                    }
                }
                break;

            case MatchState::PendingSetup:
                if (m_WhiteAgent && m_BlackAgent) {
                    static bool setupSent = false;
                    if (!setupSent) {
                        m_WhiteAgent->SendSetup("WHITE");
                        m_BlackAgent->SendSetup("BLACK");
                        setupSent = true;
                    }
                    
                    if (m_WhiteAgent->IsSetupAckReceived() && m_BlackAgent->IsSetupAckReceived()) {
                        std::cout << "[GameOrchestrator] Both agents ready. Starting game.\n";
                        m_State = MatchState::WhiteTurn;
                        m_WhiteAgent->SendMoveRequest();
                    }
                }
                break;

            case MatchState::WhiteTurn:
                HandleTurn(m_WhiteAgent, m_BlackAgent, MatchState::BlackTurn);
                break;

            case MatchState::BlackTurn:
                HandleTurn(m_BlackAgent, m_WhiteAgent, MatchState::WhiteTurn);
                break;

            case MatchState::GameOver:
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GameOrchestrator::HandleTurn(std::shared_ptr<ZMQAgentServer> activeAgent, std::shared_ptr<ZMQAgentServer> opponentAgent, MatchState nextState) {
    auto moveOpt = activeAgent->PopMoveDecision();
    if (moveOpt) {
        std::string moveStr = *moveOpt;
        std::cout << "[GameOrchestrator] Received move: " << moveStr << "\n";

        try {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            AlgebraicMove am(moveStr);
            
            std::string beforeFEN = m_Board->ToFEN();
            m_Board->Move(am);
            
            Colour opponentColor = m_Board->GetPlayerTurn();
            bool isCheck = m_Board->IsInCheck(opponentColor);
            bool missingPlus = isCheck && !(am.Flags & MoveFlag::Check) && !(am.Flags & MoveFlag::Checkmate);
            
            if (missingPlus) {
                m_Board->FromFEN(beforeFEN);
                throw IllegalMoveException(moveStr, "Move places opposing king in check but missing '+'");
            }
            
            m_GameHistory.push_back(moveStr);
            activeAgent->GetTrajectory()->SetMoveVerified(moveStr);
            
            // Check for game end
            if (!m_Board->HasLegalMoves(opponentColor)) {
                std::cout << "[GameOrchestrator] Game Over!\n";
                m_State = MatchState::GameOver;
                m_WhiteAgent->SendEndGame("DRAW", "STALEMATE"); // Simplified for now
                m_BlackAgent->SendEndGame("DRAW", "STALEMATE");
            } else {
                m_State = nextState;
                opponentAgent->SendGameHistory(m_GameHistory);
            }
        } catch (const IllegalMoveException& e) {
            std::cerr << "[GameOrchestrator] Illegal move rejected: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->SetMoveVerificationError(moveStr, std::string("Illegal move: ") + e.what());
            activeAgent->SendErrorRecovery(m_GameHistory, {e.what()});
        } catch (const InvalidAlgebraicMoveException& e) {
            std::cerr << "[GameOrchestrator] Invalid move rejected: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->SetMoveVerificationError(moveStr, std::string("Invalid move: ") + e.what());
            activeAgent->SendErrorRecovery(m_GameHistory, {e.what()});
        } catch (const std::exception& e) {
            std::cerr << "[GameOrchestrator] Fatal error during move: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->AddEvent("error", std::string("Fatal error: ") + e.what());
            std::abort();
        }
    } else {
        auto errorOpt = activeAgent->PopError();
        if (errorOpt) {
            std::string errMsg = *errorOpt;
            if (errMsg.find("did not submit a move decision") != std::string::npos) {
                std::cerr << "[GameOrchestrator] Recoverable error from agent: " << errMsg << "\n";
                activeAgent->SendErrorRecovery(m_GameHistory, {errMsg});
            } else {
                std::cerr << "[GameOrchestrator] Fatal error from agent: " << errMsg << "\n";
                std::abort();
            }
        }
    }
}
