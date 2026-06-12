#pragma once

#include <array>
#include <string>
#include <memory>
#include <atomic>

#include <SDL3/SDL.h>

#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Board/BoardAtlas.hpp"

#include "Chess/Board.h"
#include "Engine/Engine.h"
#include "Application/AgentChat/AgentSidebar.h"
#include "Application/CentralChessboardPanel.h"
#include "Application/Layout.hpp"
#include "Utility/GameOrchestrator.h"

struct ProgramArgs {
    std::string whiteEndpoint;
    std::string blackEndpoint;
    uint32_t retrospectiveRounds = 0;
};

struct TextureResources {
    std::shared_ptr<PaletteSwappedPieceAtlas> white_pieces{};
    std::shared_ptr<PaletteSwappedPieceAtlas> black_pieces{};
    std::shared_ptr<PaletteSwappedBoardAtlas> board{};
};

class Application {
public:
    Application(uint32_t width, uint32_t height, const std::string& name, const ProgramArgs& args);
    Application(const Application&) = delete;
    Application(Application&&) = delete;

    ~Application();

    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    static Application& Get() { return *s_Instance; }

    SDL_Window* GetWindow() const { return m_Window; }
    SDL_Renderer* GetRenderer() const { return m_Renderer.get(); }

    void Run();

    void UpdatePlayerColorPalette(Colour piece_color, PieceColorPaletteT palette);

private:
    void Init();
    void RenderBoard();
    void RenderImGui();
    void RenderChessPanel();
    void RenderEnginePanel(bool* show);
    void RenderSettingsPanel(bool* show);

    TextureView GetChessSprite(Piece p);

    void OnWindowClose();
    void OnWindowResize(int32_t width, int32_t height);
    void OnKeyPressed(SDL_Keycode key);
    void OnMouseButton(const SDL_MouseButtonEvent& event);

    void OnEngineUpdate(const Engine::BestContinuation& bestContinuation);

private:
    static Application* s_Instance;

    SDL_Window* m_Window = nullptr;
    std::shared_ptr<SDL_Renderer> m_Renderer = nullptr;

    struct {
        uint32_t Width, Height;
        std::string Name;
    } m_WindowProperties;

    bool m_Running = false;

    std::shared_ptr<Board> m_Board;
    std::shared_ptr<std::mutex> m_BoardMutex;
    std::shared_ptr<GameOrchestrator> m_Orchestrator;

    Square m_SelectedPiece = INVALID_SQUARE;
    bool m_IsHoldingPiece = false;  // If the selected piece follows the mouse
    BitBoard m_LegalMoves = 0;
    std::string m_BoardFEN;

    std::string m_WhiteModelName = "";
    std::string m_BlackModelName = "";
    std::atomic<bool> m_WhitePaletteNeedsUpdate = false;
    std::atomic<bool> m_BlackPaletteNeedsUpdate = false;

    TextureResources m_TextureResources;

    struct ImFont* m_HeaderFont = nullptr;

    SDL_Texture* m_BoardTargetTexture = nullptr;
    SDL_FPoint m_ChessViewportSize;
    SDL_FPoint m_BoardMousePosition;

    SDL_Color m_LegalMoveColour;
    SDL_Color m_BackgroundColour;

    std::vector<std::pair<std::string, std::filesystem::path>> m_Engines;  // Name, path
    std::unique_ptr<Engine> m_RunningEngine;
    Engine::BestContinuation m_BestContinuation;
    std::string m_BestContinuationAlgebraicMoves;

    AgentSidebar m_WhiteSidebar{ "White Agent", "WHITE" };
    AgentSidebar m_BlackSidebar{ "Black Agent", "BLACK" };
    CentralChessboardPanel m_CentralPanel;

    Layout<AgentSidebar, CentralChessboardPanel, AgentSidebar> m_Layout{ &m_WhiteSidebar, &m_CentralPanel, &m_BlackSidebar };

    std::shared_ptr<ZMQAgentServer> m_WhiteAgent;
    std::shared_ptr<ZMQAgentServer> m_BlackAgent;
    ProgramArgs m_Args;
};
