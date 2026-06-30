#pragma once

#include <array>
#include <string>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <queue>

#include <SDL3/SDL.h>
#include <so_5/wrapped_env.hpp>

#include "Graphics/GLTexture.hpp"
#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Board/BoardAtlas.hpp"

#include "Application/Model/ModelBrowserColumns.hpp"
#include "Application/Provider/ModelProviderConfiguration.hpp"
#include "Application/Command/Command.hpp"
#include "Application/Command/CommandResponse.hpp"

#include "Chess/Board.h"
#include "Engine/Engine.h"
#include "Game/Configuration/GameConfiguration.hpp"
#include "Game/GameContext.hpp"
#include "Utility/GameOrchestrator.h"
#include "Telemetry/GauntTelemetry.hpp"

namespace chess::application::model {
    struct ModelBrowserCache;
    class OpenRouterModelBrowser;
}

namespace chess::application {
    class GameView;
    enum class GameViewTheme;
}

namespace chess::application::command {
    class ZMQCommandServer;
}

// Result of opening a spectator view, returned by Application::OpenSpectatorView.
struct SpectatorViewResult {
    bool success{false};
    std::string window_title{};
    chess::application::command::OSWindowMetadata os_metadata{};
};

struct ProgramArgs {
    std::string whiteEndpoint;
    std::string blackEndpoint;
    uint32_t retrospectiveRounds = 0;
    std::string commandFilePath; // optional JSON file of startup commands
    std::string commandServerEndpoint; // optional ZMQ ROUTER endpoint for external command/query
    std::string gauntTelemetryXmlOutputFile; // optional path for Gaunt telemetry XML output
};

struct TextureResources {
    std::shared_ptr<PaletteSwappedPieceAtlas> white_pieces{};
    std::shared_ptr<PaletteSwappedPieceAtlas> black_pieces{};
    std::shared_ptr<PaletteSwappedBoardAtlas> board{};
    std::shared_ptr<chess::graphics::GLTexture> double_check_icon{};
    std::shared_ptr<chess::graphics::GLTexture> warning_icon{};
};

// Snapshot of graphics/audio backend details captured at init, surfaced in the
// Main Menu Bar > About menu.
struct SystemInfo {
    std::string gl_renderer{"(unknown)"};
    std::string gl_vendor{"(unknown)"};
    std::string gl_version{"(unknown)"};
    std::string sdl_video_driver{"(none)"};

    bool        audio_available{false};
    std::string audio_driver{"(none)"};
    std::string audio_drivers{};            // compiled-in backends, space-separated
    std::string audio_device{"(default)"};
    std::string audio_format{"(unknown)"};
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
    SDL_GLContext GetGLContext() const { return m_GLContext; }
    const TextureResources& GetTextureResources() const { return m_TextureResources; }

    void Run();

    void ToggleMainMenuBar() { m_ShowMainMenuBar = !m_ShowMainMenuBar; }
    bool IsMainMenuBarVisible() const { return m_ShowMainMenuBar; }

    chess::telemetry::GauntContextHandles* GetGauntContextHandles() { return m_GauntContextHandles.get(); }

private:
    void InitializeLogging();
    void UpdateGameAudio();
    void Init();
    void RenderImGui();
    void RenderEnginePanel(bool* show);
    void RenderSettingsPanel(bool* show);

    // Game lifecycle / viewing. These are thread-safe (guarded by
    // m_RegistryMutex) because the ExternalCommandExecutor calls them from its
    // own thread while the UI thread also touches the registry/provider store.
    std::shared_ptr<chess::game::GameContext> CreateGame(const chess::game::GameConfiguration& config, const std::string& game_id = "");
    [[nodiscard]] std::shared_ptr<chess::game::GameContext> FindGame(const std::string& game_id);
    void ConfigureProvider(const chess::application::provider::ConfiguredProvider& provider);
    [[nodiscard]] std::string ResolveApiKey(chess::game::AgentProvider provider);
    [[nodiscard]] SpectatorViewResult OpenSpectatorView(
        const std::string& game_id,
        chess::application::GameViewTheme theme,
        std::shared_ptr<chess::application::command::WindowConfiguration> window_config = nullptr);
    void ProcessPendingSpectatorViews();
    void CollectDeferredSpectatorViewMetadata();

    void OnWindowClose();
    void OnWindowResize(int32_t width, int32_t height);
    void OnKeyPressed(SDL_Keycode key);
    void OnMouseButton(const SDL_MouseButtonEvent& event);

    void OnEngineUpdate(const Engine::BestContinuation& bestContinuation);

private:
    static Application* s_Instance;
    SDL_Window* m_Window = nullptr;
    SDL_GLContext m_GLContext = nullptr;

    struct {
        uint32_t Width, Height;
        std::string Name;
    } m_WindowProperties;

    bool m_Running = false;
    bool m_ShowMainMenuBar = true;

    SystemInfo m_SystemInfo;

    std::shared_ptr<so_5::wrapped_env_t> m_GameOrchestrationEnvironment{nullptr};

    // External command/query plumbing: a single executor (shared by the startup
    // command file and the ZMQ server) and the optional ZMQ ROUTER server.
    so_5::mbox_t m_CommandExecutor{};
    std::shared_ptr<chess::application::command::ZMQCommandServer> m_CommandServer{};

    std::unordered_map<std::string, std::shared_ptr<chess::game::GameContext>> m_Games{};
    // Games awaiting main-thread MIX device creation (filled by CreateGame on any
    // thread, drained by UpdateGameAudio on the main thread). Guarded by
    // m_RegistryMutex.
    std::vector<std::shared_ptr<chess::game::GameContext>> m_PendingAudioInit{};
    // True only if the optional audio subsystem + SDL_mixer initialized.
    bool m_AudioAvailable{false};
    // Per-game audio state tracking (main thread only). Used to detect new events
    // and fire appropriate sound effects.
    struct GameAudioState {
        std::size_t capture_count{0};
        std::size_t queen_capture_count{0};
        std::size_t total_moves_count{0};  // all move attempts (accepted + rejected)
    };
    std::unordered_map<std::string, GameAudioState> m_GameAudioStates{};
    std::vector<std::unique_ptr<chess::application::GameView>> m_GameViews{};
    int m_SpectatorViewCounter{0};

    // Pending spectator view requests from the executor thread, processed on the
    // UI thread (like m_PendingAudioInit). Each entry holds the command and a
    // callback to invoke with the response once the view is created.
    struct PendingSpectatorView {
        chess::application::command::OpenSpectatorViewCommand command{};
        std::function<void(chess::application::command::OpenSpectatorViewResponse)> callback{};
    };
    std::queue<PendingSpectatorView> m_PendingSpectatorViews{};

    // Floating spectator views awaiting OS metadata collection. The viewport must
    // exist (after UpdatePlatformWindows) before we can get the native window ID.
    struct DeferredSpectatorViewMetadata {
        std::string window_title{};
        std::function<void(chess::application::command::OpenSpectatorViewResponse)> callback{};
    };
    std::vector<DeferredSpectatorViewMetadata> m_DeferredSpectatorViewMetadata{};

    std::shared_ptr<chess::application::model::ModelBrowserCache> m_ModelBrowserCache{};
    std::unique_ptr<chess::application::model::OpenRouterModelBrowser> m_OpenRouterModelBrowser{};
    chess::application::model::ModelBrowserColumns m_ModelBrowserColumns{};
    chess::application::provider::ProviderConfigurationStore m_ProviderStore{};

    // Guards m_Games and m_ProviderStore against concurrent access by the UI
    // thread and the ExternalCommandExecutor thread.
    std::mutex m_RegistryMutex;

    // Scratch board behind the debug FEN window (not tied to a specific game).
    std::shared_ptr<Board> m_Board;
    std::shared_ptr<std::mutex> m_BoardMutex;
    std::string m_BoardFEN;

    TextureResources m_TextureResources;

    struct ImFont* m_HeaderFont = nullptr;

    SDL_FPoint m_ChessViewportSize;
    SDL_FPoint m_BoardMousePosition;

    SDL_Color m_LegalMoveColour;
    SDL_Color m_BackgroundColour;

    std::vector<std::pair<std::string, std::filesystem::path>> m_Engines;  // Name, path
    std::unique_ptr<Engine> m_RunningEngine;
    Engine::BestContinuation m_BestContinuation;
    std::string m_BestContinuationAlgebraicMoves;

    ProgramArgs m_Args;

    std::shared_ptr<chess::telemetry::GauntContextHandles> m_GauntContextHandles{};
};
