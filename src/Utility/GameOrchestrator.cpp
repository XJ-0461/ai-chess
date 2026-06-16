#ifdef IGNORE_THIS_FILE_MIGRATING_AWAY_FROM_THIS_FILE_

#include "GameOrchestrator.h"

#include "../Chess/ChessException.h"
#include <iostream>
#include <chrono>

GameOrchestrator::GameOrchestrator(
    std::shared_ptr<ZMQAgentServer> whiteAgent,
    std::shared_ptr<ZMQAgentServer> blackAgent,
    std::shared_ptr<Board> board,
    std::shared_ptr<std::mutex> boardMutex,
    uint32_t retrospectiveRounds
) : m_WhiteAgent(whiteAgent), m_BlackAgent(blackAgent), m_Board(board), m_BoardMutex(boardMutex), 
    m_State(MatchState::Handshake), m_RetrospectiveRounds(retrospectiveRounds) {
}

GameOrchestrator::~GameOrchestrator() {
    Stop();
}

void GameOrchestrator::Start() {
    m_Thread = std::jthread([this](std::stop_token st) {
        OrchestratorLoop(st);
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
                        m_WhiteAgent->Send(chess::agent::message::Ping{});
                        m_BlackAgent->Send(chess::agent::message::Ping{});
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
                        m_WhiteAgent->Send(chess::agent::message::SetupRequest{
                            .color = "WHITE", 
                            .enable_quip = true,
                            .enable_draw_offer = true,
                            .enable_resignation = true
                        });
                        m_BlackAgent->Send(chess::agent::message::SetupRequest{
                            .color = "BLACK", 
                            .enable_quip = true,
                            .enable_draw_offer = true,
                            .enable_resignation = true
                        });
                        setupSent = true;
                    }
                    
                    if (m_WhiteAgent->IsSetupAckReceived() && m_BlackAgent->IsSetupAckReceived()) {
                        std::cout << "[GameOrchestrator] Both agents ready. Starting game.\n";
                        m_State = MatchState::WhiteTurn;
                        m_WhiteAgent->Send(chess::agent::message::StartMoveRequest{
                            .game_history = {}, 
                            .opponent_quips = {},
                            .personality = m_WhiteAgent->GetPersonality(),
                            .turn_number = 1
                        });
                    }
                }
                break;

            case MatchState::WhiteTurn:
                HandleTurn(m_WhiteAgent, m_BlackAgent, MatchState::BlackTurn);
                break;

            case MatchState::BlackTurn:
                HandleTurn(m_BlackAgent, m_WhiteAgent, MatchState::WhiteTurn);
                break;

            case MatchState::DrawOffer:
                HandleDrawOffer();
                break;

            case MatchState::WhiteRetrospective:
                HandleRetrospective(m_WhiteAgent, m_BlackAgent, MatchState::BlackRetrospective);
                break;

            case MatchState::BlackRetrospective:
                HandleRetrospective(m_BlackAgent, m_WhiteAgent, MatchState::WhiteRetrospective);
                break;

            case MatchState::GameOver:
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GameOrchestrator::HandleTurn(std::shared_ptr<ZMQAgentServer> activeAgent, std::shared_ptr<ZMQAgentServer> opponentAgent, MatchState nextState) {
    auto reqOpt = activeAgent->PopBoardStateRequest();
    if (reqOpt) {
        nlohmann::json responseData;
        {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            responseData["board_state"] = m_Board->ToFEN();

            std::vector<std::string> whitePieces;
            std::vector<std::string> blackPieces;

            for (int i = 0; i < 64; ++i) {
                Piece p = (*m_Board)[i];
                if (p != None) {
                    char file = 'a' + FileOf(i);
                    char rank = '1' + RankOf(i);
                    std::string square = { file, rank };

                    std::string pieceName = "";
                    switch (GetPieceType(p)) {
                        case Pawn:   pieceName = "Pawn"; break;
                        case Knight: pieceName = "Knight"; break;
                        case Bishop: pieceName = "Bishop"; break;
                        case Rook:   pieceName = "Rook"; break;
                        case Queen:  pieceName = "Queen"; break;
                        case King:   pieceName = "King"; break;
                        default:     pieceName = "Unknown"; break;
                    }

                    if (GetColour(p) == White) {
                        whitePieces.push_back(square + ": " + pieceName);
                    } else {
                        blackPieces.push_back(square + ": " + pieceName);
                    }
                }
            }

            responseData["white_state"]["active_pieces"] = whitePieces;
            responseData["black_state"]["active_pieces"] = blackPieces;
        }
        activeAgent->Send(chess::agent::message::GetBoardStateResponse{.id = reqOpt->id, .data = responseData});
    }

    if (activeAgent->PopResign()) {
        std::cout << "[GameOrchestrator] " << (activeAgent == m_WhiteAgent ? "WHITE" : "BLACK") << " resigned.\n";
        m_Winner = (activeAgent == m_WhiteAgent ? "BLACK" : "WHITE");
        m_EndCause = "RESIGNATION";
        m_State = MatchState::GameOver;
        m_WhiteAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
        m_BlackAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
        return;
    }

    if (activeAgent->PopOfferDraw()) {
        std::cout << "[GameOrchestrator] " << (activeAgent == m_WhiteAgent ? "WHITE" : "BLACK") << " offered a draw.\n";
        m_PreDrawState = m_State;
        m_State = MatchState::DrawOffer;
        
        // Collect quips from proposer to send to recipient
        std::vector<std::string> proposerQuips;
        auto q_queue = activeAgent->GetQuipsQueue();
        if (q_queue) {
            std::string q;
            while (q_queue->try_dequeue(q)) proposerQuips.push_back(q);
        }

        opponentAgent->Send(chess::agent::message::DrawOfferRequest{
            .game_history = m_GameHistory,
            .opponent_quips = proposerQuips,
            .personality = opponentAgent->GetPersonality()
        });
        return;
    }

    auto moveOpt = activeAgent->PopMoveDecision();
    if (moveOpt) {
        std::string moveStr = *moveOpt;
        std::cout << "[GameOrchestrator] Received move: " << moveStr << "\n";

        try {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            AlgebraicMove am(moveStr);
            
            std::string beforeFEN = m_Board->ToFEN();
            m_Board->Move(am);

            const Colour opponentColor = m_Board->GetPlayerTurn();

            const bool isCheckMate = m_Board->IsInCheckmate(opponentColor);
            const bool missingMate = isCheckMate && !(am.Flags & MoveFlag::Checkmate);
            const bool extraMate = !isCheckMate && (am.Flags & MoveFlag::Checkmate);
            
            if (missingMate) {
                m_Board->FromFEN(beforeFEN);
                throw IllegalMoveException(moveStr, "Move checkmates opposing king but missing '#'");
            }
            if (extraMate) {
                m_Board->FromFEN(beforeFEN);
                throw IllegalMoveException(moveStr, "Move includes '#' but does not place king in checkmate");
            }

            const bool isCheck = m_Board->IsInCheck(opponentColor);
            const bool missingPlus = isCheck && !isCheckMate && !(am.Flags & MoveFlag::Check);
            const bool extraPlus = !isCheck && (am.Flags & MoveFlag::Check);
            
            if (missingPlus) {
                m_Board->FromFEN(beforeFEN);
                throw IllegalMoveException(moveStr, "Move places opposing king in check but missing '+'");
            }
            if (extraPlus) {
                m_Board->FromFEN(beforeFEN);
                throw IllegalMoveException(moveStr, "Move includes '+' but does not place king in check");
            }

            m_GameHistory.push_back(moveStr);
            activeAgent->GetTrajectory()->SetMoveVerified(moveStr);
            
            // Check for game end
            if (!m_Board->HasLegalMoves(opponentColor)) {
                std::cout << "[GameOrchestrator] Game Over!\n";
                
                if (isCheckMate) {
                    m_Winner = opponentColor == White ? "BLACK" : "WHITE";
                    m_EndCause = "CHECKMATE";
                } else {
                    m_Winner = "DRAW";
                    m_EndCause = "STALEMATE";
                }

                if (m_RetrospectiveRounds > 0) {
                    m_State = opponentColor == White ? MatchState::WhiteRetrospective : MatchState::BlackRetrospective;
                    m_CurrentRetrospectiveRound = 0;
                    
                    // Transition quips for the first retrospective turn
                    std::vector<std::string> initial_quips{};
                    std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> q_queue = activeAgent->GetQuipsQueue();
                    if (q_queue) {
                        std::string q;
                        while (q_queue->try_dequeue(q)) {
                            initial_quips.push_back(q);
                        }
                    }
                    opponentAgent->Send(chess::agent::message::RetrospectiveRequest{
                        .game_history = m_GameHistory,
                        .opponent_quips = initial_quips,
                        .winner = m_Winner,
                        .cause = m_EndCause
                    });
                } else {
                    m_State = MatchState::GameOver;
                    m_WhiteAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
                    m_BlackAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
                }
            } else {
                m_State = nextState;
                std::vector<std::string> new_quips{};
                std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> quips_queue{nullptr};
                if (opponentColor == White) {
                    quips_queue = m_BlackAgent->GetQuipsQueue();
                } else {
                    quips_queue = m_WhiteAgent->GetQuipsQueue();
                }
                if (quips_queue) {
                    const std::size_t estimated_queue_size = quips_queue->size_approx();
                    new_quips.reserve(estimated_queue_size);
                    std::ranges::fill_n(std::back_inserter(new_quips), static_cast<std::int64_t>(estimated_queue_size), std::string{});
                    const std::size_t dequeued_count = quips_queue->try_dequeue_bulk(new_quips.data(), estimated_queue_size);
                    new_quips.resize(dequeued_count);
                }
                opponentAgent->Send(chess::agent::message::StartMoveRequest{
                    .game_history = m_GameHistory, 
                    .opponent_quips = new_quips,
                    .personality = opponentAgent->GetPersonality(),
                    .turn_number = static_cast<std::uint32_t>((m_GameHistory.size() / 2) + 1)
                });
            }
        } catch (const IllegalMoveException& e) {
            std::cerr << "[GameOrchestrator] Illegal move rejected: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->SetMoveVerificationError(moveStr, std::string("Illegal move: ") + e.what());
            activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                .game_history = m_GameHistory, 
                .opponent_quips = {},
                .errors = {e.what()},
                .personality = activeAgent->GetPersonality()
            });
        } catch (const InvalidAlgebraicMoveException& e) {
            std::cerr << "[GameOrchestrator] Invalid move rejected: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->SetMoveVerificationError(moveStr, std::string("Invalid move: ") + e.what());
            activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                .game_history = m_GameHistory, 
                .opponent_quips = {},
                .errors = {e.what()},
                .personality = activeAgent->GetPersonality()
            });
        } catch (const InvalidPieceTypeException& e) {
            std::cerr << "[GameOrchestrator] Invalid piece type: " << moveStr << "\n";
            activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                .game_history = m_GameHistory, 
                .opponent_quips = {},
                .errors = {e.what()},
                .personality = activeAgent->GetPersonality()
            });
        } catch (const InvalidLongAlgebraicMoveException& e) {
            std::cerr << "[GameOrchestrator] Invalid long move: " << moveStr << "\n";
            activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                .game_history = m_GameHistory, 
                .opponent_quips = {},
                .errors = {e.what()},
                .personality = activeAgent->GetPersonality()
            });
        } catch (const std::exception& e) {
            std::cerr << "[GameOrchestrator] Fatal error during move: " << moveStr << " (" << e.what() << ")\n";
            activeAgent->GetTrajectory()->AddEvent(chess::agent::InfoEvent{.message = std::string("Fatal error: ") + e.what(), .isError = true});
            std::abort();
        }
    } else {
        auto errorOpt = activeAgent->PopError();
        if (errorOpt) {
            std::string errMsg = *errorOpt;
            if (errMsg.find("did not submit a move decision") != std::string::npos) {
                std::cerr << "[GameOrchestrator] Recoverable error from agent: " << errMsg << "\n";
                activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                    .game_history = m_GameHistory, 
                    .opponent_quips = {},
                    .errors = {errMsg},
                    .personality = activeAgent->GetPersonality()
                });
            } else if (errMsg.find("Invalid final response") != std::string::npos) {
                std::cerr << "[GameOrchestrator] Recoverable error from agent: " << errMsg << "\n";
                activeAgent->Send(chess::agent::message::ErrorRecoveryRequest{
                    .game_history = m_GameHistory,
                    .opponent_quips = {},
                    .errors = {errMsg},
                    .personality = activeAgent->GetPersonality()
                });
            } else {
                std::cerr << "[GameOrchestrator] Fatal error from agent: " << errMsg << "\n";
                std::abort();
            }
        }
    }
}

void GameOrchestrator::HandleDrawOffer() {
    std::shared_ptr<ZMQAgentServer> proposer = (m_PreDrawState == MatchState::WhiteTurn ? m_WhiteAgent : m_BlackAgent);
    std::shared_ptr<ZMQAgentServer> recipient = (m_PreDrawState == MatchState::WhiteTurn ? m_BlackAgent : m_WhiteAgent);

    auto decision = recipient->PopDrawDecision();
    if (decision) {
        if (*decision) {
            std::cout << "[GameOrchestrator] Draw offer ACCEPTED by recipient.\n";
            m_Winner = "DRAW";
            m_EndCause = "AGREEMENT";
            m_State = MatchState::GameOver;
            m_WhiteAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
            m_BlackAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
        } else {
            std::cout << "[GameOrchestrator] Draw offer DECLINED by recipient.\n";
            m_State = m_PreDrawState;

            // Collect quips from recipient to send back to proposer
            std::vector<std::string> recipientQuips;
            auto q_queue = recipient->GetQuipsQueue();
            if (q_queue) {
                std::string q;
                while (q_queue->try_dequeue(q)) recipientQuips.push_back(q);
            }

            proposer->Send(chess::agent::message::DrawOfferDeclinedRequest{
                .game_history = m_GameHistory,
                .opponent_quips = recipientQuips,
                .personality = proposer->GetPersonality()
            });
        }
    }
}

void GameOrchestrator::HandleRetrospective(
    std::shared_ptr<ZMQAgentServer> activeAgent,
    std::shared_ptr<ZMQAgentServer> opponentAgent,
    MatchState nextState
) {
    auto endTurnOpt = activeAgent->PopEndTurn();
    if (endTurnOpt) {
        if (nextState == MatchState::WhiteRetrospective) {
            m_CurrentRetrospectiveRound = m_CurrentRetrospectiveRound + 1;
            if (m_CurrentRetrospectiveRound >= m_RetrospectiveRounds) {
                m_State = MatchState::GameOver;
                m_WhiteAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
                m_BlackAgent->Send(chess::agent::message::GameEnd{.winner = m_Winner, .cause = m_EndCause});
                return;
            }
        }
        
        m_State = nextState;
        
        // Pass quips to next agent
        std::vector<std::string> next_quips{};
        std::shared_ptr<moodycamel::ConcurrentQueue<std::string>> q_queue = activeAgent->GetQuipsQueue();
        if (q_queue) {
            std::string q;
            while (q_queue->try_dequeue(q)) next_quips.push_back(q);
        }
        opponentAgent->Send(chess::agent::message::RetrospectiveRequest{
            .game_history = m_GameHistory,
            .opponent_quips = next_quips,
            .winner = m_Winner,
            .cause = m_EndCause
        });
    }
}

#endif