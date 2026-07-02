
#include <iostream>
#include <utility>
#include <variant>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "Application.h"
#include "Resources.h"
#include "Application/Task/TaskExecutor.hpp"
#include "Application/Model/ModelBrowserCache.hpp"
#include "Application/Model/OpenRouterModelBrowser.hpp"
#include "ModelBrowserWindow.h"
#include "ModelProviderConfigurationWindow.h"
#include "ProvidersWindow.h"
#include "ConfigureGameWindow.h"
#include "GameBrowserWindow.h"
#include "GameView.h"
#include "Application/Command/CommandFile.hpp"
#include "Application/Command/ExternalCommandExecutor.hpp"
#include "Utility/ZMQCommandServer.hpp"

#ifdef __linux__
#include "Platform/Linux/LinuxWindowInfo.hpp"
#endif

#include <nlohmann/json.hpp>
#include "Game/Execution/GameOrchestrator.hpp"
#include "Game/GameAudio.hpp"
#include "Telemetry/GameTelemetry.hpp"
#include "Chess/Board.h"
#include "Graphics/Sound/Confirm01.hpp"
#include "Graphics/Sound/CollectPoint2.hpp"
#include "Graphics/Sound/FireHit01.hpp"
#include "Graphics/Sound/ActivatePower.hpp"
#include "Graphics/Sound/NoEntry.hpp"
#include "Graphics/Font/PixelOperatorSCBold.hpp"
#include "Graphics/Icon/CheckDoubleSVG.hpp"
#include "Graphics/Icon/WarningSVG.hpp"
#include "Graphics/SVG/Import.hpp"
#include "Graphics/Theme/Color/DarkAgentSidebarColorPalette.hpp"
#include "Graphics/Theme/Color/WoodAgentSidebarColorPalette.hpp"

Application* Application::s_Instance = nullptr;

// Creates the "stdout_chess" logger used by the CHESS_*_TRACE macros. The
// payload handed to spdlog is already a JSON object (from chess::log::MakeLog),
// so the pattern embeds it verbatim as the "record" value, yielding one
// self-contained structured JSON object per line, e.g.
//   {"timestamp":"...","level":"trace","record":{"function":"OnTurnTransition","operation":"enter"}}
// A plain (non-colour) sink is used so no ANSI escape codes corrupt the JSON.
void Application::InitializeLogging() {
    if (spdlog::get("stdout_chess")) {
        return; // already initialised
    }
    auto logger = spdlog::stdout_logger_mt("stdout_chess");
    logger->set_pattern(R"({"timestamp":"%Y-%m-%dT%H:%M:%S.%e%z","level":"%l","record":%v})");
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
}

Application::Application(uint32_t width, uint32_t height, const std::string& name, const ProgramArgs& args)
    : m_WindowProperties{ width, height, name },
        m_ChessViewportSize{static_cast<float>(width), static_cast<float>(height)},
        m_Args(args),
        m_Board(std::make_shared<Board>()),
        m_BoardMutex(std::make_shared<std::mutex>()) {

    if (!s_Instance) {
        s_Instance = this;
    }

    InitializeLogging();

    // Initialize Gaunt Telemetry
    {
        chess::telemetry::GauntTelemetryConfig config;
        if (!m_Args.gauntTelemetryXmlOutputFile.empty()) {
            config.xml_output_file = m_Args.gauntTelemetryXmlOutputFile;
        }
        config.enable_console_logging = true;

        const std::string session_id = chess::telemetry::GenerateSessionId();
        m_GauntContextHandles = chess::telemetry::InitializeGauntTelemetry(config, session_id);
    }

    // SDL_Init replaces glfwInit. Video + events are required.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cout << "Could not initialize SDL: " << SDL_GetError() << "\n";
        return;
    }

    // Audio is OPTIONAL: a headless box / VM may have no output device. A
    // failure here must never abort the app (it previously took the whole
    // process down). When unavailable, all audio paths are skipped.
    // Compiled-in audio backends, captured regardless of init success (this is
    // what distinguishes "no backend compiled in" from "couldn't reach a server").
    {
        const int driver_count = SDL_GetNumAudioDrivers();
        for (int i = 0; i < driver_count; ++i) {
            if (i != 0) {
                m_SystemInfo.audio_drivers += ' ';
            }
            m_SystemInfo.audio_drivers += SDL_GetAudioDriver(i);
        }
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) && MIX_Init()) {
        m_AudioAvailable = true;
        m_SystemInfo.audio_available = true;
        if (const char* const driver = SDL_GetCurrentAudioDriver()) {
            m_SystemInfo.audio_driver = driver;
        }
        SDL_AudioSpec spec{};
        int sample_frames = 0;
        if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &sample_frames)) {
            m_SystemInfo.audio_format =
                std::to_string(spec.freq) + " Hz, " +
                std::to_string(spec.channels) + " ch, " +
                std::to_string(SDL_AUDIO_BITSIZE(spec.format)) + "-bit";
        }
        if (const char* const device_name = SDL_GetAudioDeviceName(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)) {
            m_SystemInfo.audio_device = device_name;
        }
    } else {
        std::cout << "Audio unavailable (" << SDL_GetError() << "); continuing without sound. Drivers: "
                  << m_SystemInfo.audio_drivers << "\n";
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    // We keep SDL3 as the platform backend (window, input, clipboard, ...) but
    // render through Dear ImGui's OpenGL3 backend, which - unlike the
    // SDL_Renderer backend - supports multi-viewport (multiple OS windows).
    // Request a GL context matching the OpenGL3 backend's expectations.
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char* glsl_version = "#version 330 core";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_Window = SDL_CreateWindow(name.c_str(), 966, 600, window_flags);
    if (!m_Window) {
        std::cout << "Could not create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return;
    }

    m_GLContext = SDL_GL_CreateContext(m_Window);
    if (!m_GLContext) {
        std::cout << "Could not create GL context: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(m_Window);
        SDL_Quit();
        return;
    }
    SDL_GL_MakeCurrent(m_Window, m_GLContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Capture the GL implementation details for the About > Graphics menu (a
    // software fallback like llvmpipe is then visible there rather than buried
    // in console output).
    if (const GLubyte* const gl_renderer = glGetString(GL_RENDERER)) {
        m_SystemInfo.gl_renderer = reinterpret_cast<const char*>(gl_renderer);
    }
    if (const GLubyte* const gl_vendor = glGetString(GL_VENDOR)) {
        m_SystemInfo.gl_vendor = reinterpret_cast<const char*>(gl_vendor);
    }
    if (const GLubyte* const gl_version = glGetString(GL_VERSION)) {
        m_SystemInfo.gl_version = reinterpret_cast<const char*>(gl_version);
    }
    if (const char* const video_driver = SDL_GetCurrentVideoDriver()) {
        m_SystemInfo.sdl_video_driver = video_driver;
    }

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = "resources/default_imgui_ini.ini";

    // Setup Platform/Renderer backends: SDL3 platform + OpenGL3 renderer.
    ImGui_ImplSDL3_InitForOpenGL(m_Window, m_GLContext);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

Application::~Application() {
    // Tear down in dependency order: every thread that PRODUCES telemetry must be
    // fully stopped before the telemetry contexts it writes to are destroyed, and
    // the actor environment must be explicitly stopped so the process can exit
    // once the window closes (otherwise its threads keep the process alive).

    // 1. Stop accepting external commands (joins the ZMQ ROUTER server thread).
    if (m_CommandServer) {
        m_CommandServer->Stop();
        m_CommandServer.reset();
    }

    // 2. Stop the running analysis engine, if any (joins its I/O thread).
    if (m_RunningEngine) {
        m_RunningEngine->Stop();
        m_RunningEngine.reset();
    }

    // 3. Drop the games + views, then stop the actor environment. This joins the
    //    orchestrator, TaskExecutor and per-player request threads — the
    //    producers of telemetry — so nothing can emit after this returns.
    //    (m_GameViews own per-view GL atlases; freed here while the context is
    //    still current.)
    m_GameViews.clear();
    m_Games.clear();
    if (m_GameOrchestrationEnvironment) {
        m_GameOrchestrationEnvironment->stop();
        m_GameOrchestrationEnvironment->join();
        m_GameOrchestrationEnvironment.reset();
    }

    // 4. Now that no thread can write telemetry, shut it down.
    if (m_GauntContextHandles) {
        chess::telemetry::ShutdownGauntTelemetry(*m_GauntContextHandles);
        m_GauntContextHandles.reset();
    }

    // 5. GL / audio / SDL teardown. GL textures must be released while their
    //    context is still current; per-game MIX mixers (GameAudio dtors ran in
    //    m_Games.clear() above) are gone before SDL_mixer is torn down.
    m_TextureResources = {};  // shared icon textures
    if (m_AudioAvailable) {
        MIX_Quit();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(m_GLContext);
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

void Application::Run() {
    m_Running = true;

    Init();

    while (m_Running) {
        // SDL_PollEvent replaces glfwPollEvents. Required for input and window events.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            
            if (event.type == SDL_EVENT_QUIT)
                m_Running = false;
            
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window))
                m_Running = false;
            
            if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(m_Window))
                OnWindowResize(event.window.data1, event.window.data2);
            
            if (event.type == SDL_EVENT_KEY_DOWN)
                OnKeyPressed(event.key.key);
            
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
                OnMouseButton(event.button);
        }

        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Open MIX devices for new games + play queued game SFX (main thread).
        UpdateGameAudio();

        // Process pending spectator view requests from the executor thread.
        ProcessPendingSpectatorViews();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        
        {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            m_BoardFEN = m_Board->ToFEN();
        }

        RenderImGui();

        ImGui::Render();

        const ImGuiIO& io = ImGui::GetIO();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(
            m_BackgroundColour.r / 255.0f,
            m_BackgroundColour.g / 255.0f,
            m_BackgroundColour.b / 255.0f,
            m_BackgroundColour.a / 255.0f
        );
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render and present additional platform windows (multi-viewport). The
        // GL context made current by the platform backend must be restored
        // before swapping our main window's buffers.
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window* const backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext const backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);

            // Collect OS metadata for floating windows now that their viewports exist.
            CollectDeferredSpectatorViewMetadata();
        }

        // SDL_GL_SwapWindow replaces glfwSwapBuffers / SDL_RenderPresent.
        SDL_GL_SwapWindow(m_Window);
    }
}

void Application::Init() {
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;

    void* font = (void*)Resources::Fonts::Roboto::ROBOTO_REGULAR;
    std::int32_t fontSize = sizeof(Resources::Fonts::Roboto::ROBOTO_REGULAR);
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(font, fontSize, 20.0f, &fontConfig);

    m_HeaderFont = io.Fonts->AddFontFromMemoryTTF((void*)kPixelOperatorSCBoldTFFBytes, sizeof(kPixelOperatorSCBoldTFFBytes), 24.0f, &fontConfig);

    if (!std::filesystem::exists("imgui.ini")) {
        ImGui::LoadIniSettingsFromMemory(Resources::DEFAULT_IMGUI_INI);
    }

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.GrabRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.WindowMinSize = { 200.0f, 200.0f };

    // Hide the resize-grip triangle in the bottom-right corner of windows
    // (resizing still works via the window edges) for a cleaner look.
    style.Colors[ImGuiCol_ResizeGrip]        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // With multi-viewport enabled, detached windows render as real OS windows.
    // Keep them opaque and square-cornered so each can be isolated/recorded
    // individually (e.g. selecting a single window in gpu-screen-recorder).
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Shared, app-level icon textures used by agent chat bubbles. Per-game
    // piece/board atlases are owned by each spawned GameView instead.
    m_TextureResources.double_check_icon = chess::graphics::svg::LoadSVGTextureFromMemory(kCheckDoubleSvg);
    m_TextureResources.warning_icon = chess::graphics::svg::LoadSVGTextureFromMemory(kWarningSvg);

    m_LegalMoveColour = { 255, 0, 255, 127 };
    m_BackgroundColour = { 51, 51, 51, 255 };

    m_BoardFEN = m_Board->ToFEN();

    m_GameOrchestrationEnvironment = std::make_shared<so_5::wrapped_env_t>();

    // Stand up the shared, bounded TaskExecutor pool so any component can submit
    // long-running BlockingTask jobs without spawning unbounded threads.
    chess::application::task::IntroduceTaskExecutor(m_GameOrchestrationEnvironment->environment());

    // Model browsing: a shared cache plus provider-specific browsers that fetch
    // asynchronously through the TaskExecutor pool.
    m_ModelBrowserCache = std::make_shared<chess::application::model::ModelBrowserCache>();
    m_OpenRouterModelBrowser = std::make_unique<chess::application::model::OpenRouterModelBrowser>(
        m_GameOrchestrationEnvironment->environment(),
        m_ModelBrowserCache
    );

    // External command/query plumbing. A single ExternalCommandExecutor actor
    // (its own thread, runs commands sequentially) is shared by the optional
    // startup command file and the optional ZMQ command/query server.
    const bool want_executor = !m_Args.commandFilePath.empty() || !m_Args.commandServerEndpoint.empty();
    if (want_executor) {
        chess::application::command::CommandTargetCallbacks callbacks{
            .configure_provider = [this](const chess::application::provider::ConfiguredProvider& provider) {
                ConfigureProvider(provider);
            },
            .create_game = [this](const chess::game::GameConfiguration& config, const std::string& id) {
                return CreateGame(config, id);
            },
            .find_game = [this](const std::string& id) {
                return FindGame(id);
            },
            .open_spectator_view = [this](
                const chess::application::command::OpenSpectatorViewCommand& command,
                std::function<void(chess::application::command::OpenSpectatorViewResponse)> callback
            ) {
                // Enqueue the request for the UI thread to process.
                std::lock_guard<std::mutex> lock(m_RegistryMutex);
                m_PendingSpectatorViews.push(PendingSpectatorView{command, std::move(callback)});
            },
            .set_move_history_bar = [this](const std::string& window_id, bool enable) -> bool {
                std::lock_guard<std::mutex> lock(m_RegistryMutex);
                for (auto& view : m_GameViews) {
                    if (view->WindowTitle() == window_id) {
                        view->SetMoveHistoryBarEnabled(enable);
                        return true;
                    }
                }
                return false;
            },
        };
        m_CommandExecutor = chess::application::command::IntroduceExternalCommandExecutor(
            m_GameOrchestrationEnvironment->environment(), std::move(callbacks));
    }

    // Optional startup automation: load an ordered command file and feed it to
    // the executor (fire-and-forget, no reply).
    if (!m_Args.commandFilePath.empty()) {
        std::string error;
        auto commands = chess::application::command::LoadCommandsFromFile(m_Args.commandFilePath, error);
        if (commands) {
            std::cout << "Loaded " << commands->size() << " startup command(s) from " << m_Args.commandFilePath << "\n";
            for (auto& command : *commands) {
                so_5::send<chess::application::command::ExecuteCommand>(m_CommandExecutor, std::move(command));
            }
        } else {
            std::cerr << "Failed to load startup commands: " << error << "\n";
        }
    }

    // Optional ZMQ ROUTER server: external apps send JSON requests, which are
    // forwarded into the command queue; results are routed back asynchronously,
    // correlated by the request 'id' and the ROUTER identity.
    if (!m_Args.commandServerEndpoint.empty()) {
        m_CommandServer = std::make_shared<chess::application::command::ZMQCommandServer>(
            chess::application::command::ZMQCommandServerOptions{ m_Args.commandServerEndpoint });

        // weak_ptr (not raw/shared) avoids an ownership cycle — the server owns
        // the handler — while still guarding against the shutdown race where a
        // reply is produced after the server has been torn down.
        const std::weak_ptr<chess::application::command::ZMQCommandServer> weak_server = m_CommandServer;
        const so_5::mbox_t executor = m_CommandExecutor;
        m_CommandServer->SetRequestHandler(
            [weak_server, executor](chess::application::command::ClientIdentity identity, std::string payload) {
                const auto server = weak_server.lock();
                if (!server) {
                    return;
                }
                nlohmann::json request;
                try {
                    request = nlohmann::json::parse(payload);
                } catch (const std::exception&) {
                    server->Reply(std::move(identity),
                        nlohmann::json{{"type", "error"}, {"error", "invalid_json"}}.dump());
                    return;
                }

                const std::string type = request.value("type", std::string{});
                const std::string id = request.value("id", std::string{});

                // Runtime requests carry their arguments in a `detail` object
                // (matching the startup command-file format). For backward
                // compatibility, if `detail` is absent, fall back to a top-level
                // `game_id` (older query_match_result callers used that shape).
                nlohmann::json detail = request.value("detail", nlohmann::json::object());
                if (!request.contains("detail") && request.contains("game_id")) {
                    detail["game_id"] = request.at("game_id");
                }

                // Routes a result back to the originating client, tagging it with
                // the request's correlation id. Runs on the executor thread.
                auto reply = [weak_server, identity, id](const nlohmann::json& result) {
                    const auto reply_server = weak_server.lock();
                    if (!reply_server) {
                        return;
                    }
                    nlohmann::json out = result;
                    if (!id.empty()) {
                        out["id"] = id;
                    }
                    reply_server->Reply(identity, out.dump());
                };

                // Parse and dispatch any supported command (the same surface as
                // the startup command file). The executor answers each with a
                // response/ack, routed back correlated by `id`.
                std::string parse_error;
                auto command = chess::application::command::ParseCommand(type, detail, parse_error);
                if (command) {
                    so_5::send<chess::application::command::ExecuteCommand>(
                        executor, std::move(*command), reply);
                } else {
                    nlohmann::json error{{"type", "error"}, {"error", parse_error}};
                    if (!id.empty()) {
                        error["id"] = id;
                    }
                    server->Reply(std::move(identity), error.dump());
                }
            });

        m_CommandServer->Start();
        std::cout << "External command server listening on " << m_Args.commandServerEndpoint << "\n";
    }
}

void Application::RenderImGui() {
    static bool s_ShowSettingsWindow = false, s_ShowFENWindow = false, s_ShowEngineWindow = false;
    static bool s_ShowConfigureGameWindow = false, s_ShowGameBrowserWindow = false;
    static bool s_ShowModelBrowserWindow = false;
    static bool s_ShowProviderConfigWindow = false, s_ShowProvidersWindow = false;
    static chess::game::GameConfiguration s_NewGameConfig{};

    if (m_ShowMainMenuBar && ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Configure New Game..")) {
                s_ShowConfigureGameWindow = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                m_Running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Game Browser")) {
                s_ShowGameBrowserWindow = true;
            }
            if (ImGui::BeginMenu("Model Browser")) {
                if (ImGui::MenuItem("Show")) {
                    s_ShowModelBrowserWindow = true;
                }
                if (ImGui::BeginMenu("Configure")) {
                    if (ImGui::BeginMenu("Columns")) {
                        chess::application::RenderModelBrowserColumnsMenu(m_ModelBrowserColumns);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Providers")) {
                s_ShowProvidersWindow = true;
            }
            if (ImGui::MenuItem("Configure Provider...")) {
                s_ShowProviderConfigWindow = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Colours")) {
                s_ShowSettingsWindow = true;
            }
            if (ImGui::MenuItem("FEN")) {
                s_ShowFENWindow     = true;
            }
            if (ImGui::MenuItem("Engine")) {
                s_ShowEngineWindow  = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About")) {
            ImGui::Text("SDL3 + OpenGL3 Backend");
            ImGui::Separator();
            if (ImGui::BeginMenu("Graphics")) {
                ImGui::Text("Renderer: %s", m_SystemInfo.gl_renderer.c_str());
                ImGui::Text("Vendor: %s", m_SystemInfo.gl_vendor.c_str());
                ImGui::Text("Version: %s", m_SystemInfo.gl_version.c_str());
                ImGui::Text("SDL video driver: %s", m_SystemInfo.sdl_video_driver.c_str());
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                ImGui::Text("Status: %s", m_SystemInfo.audio_available ? "available" : "unavailable");
                ImGui::Text("Current driver: %s", m_SystemInfo.audio_driver.c_str());
                ImGui::Text("Compiled-in drivers: %s", m_SystemInfo.audio_drivers.c_str());
                ImGui::Text("Default device: %s", m_SystemInfo.audio_device.c_str());
                ImGui::Text("Default format: %s", m_SystemInfo.audio_format.c_str());
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    chess::application::RenderConfigureGameWindow(&s_ShowConfigureGameWindow, s_NewGameConfig,
        [this](const chess::game::GameConfiguration& config) { CreateGame(config); });

    const chess::application::GameBrowserCallbacks game_browser_callbacks{
        .on_play = [this](const std::string& id) -> void {
            const auto context = FindGame(id);
            if (context) {
                so_5::send<chess::game::execution::StartGame>(context->command_mbox);
            }
        },
        .on_open_spectator = [this](const std::string& id) -> SpectatorViewResult {
            return OpenSpectatorView(id, chess::application::GameViewTheme::Default);
        },
        .on_open_spectator_high_contrast = [this](const std::string& id) -> SpectatorViewResult {
            return OpenSpectatorView(id, chess::application::GameViewTheme::HighContrast);
        },
    };

    // Snapshot the registry under lock (cheap shared_ptr copies) so the browser
    // renders without racing the ExternalCommandExecutor thread.
    std::unordered_map<std::string, std::shared_ptr<chess::game::GameContext>> games_snapshot;
    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        games_snapshot = m_Games;
    }
    chess::application::RenderGameBrowserWindow(&s_ShowGameBrowserWindow, games_snapshot, game_browser_callbacks);

    chess::application::RenderModelBrowserWindow(&s_ShowModelBrowserWindow, *m_ModelBrowserCache, *m_OpenRouterModelBrowser, m_ModelBrowserColumns);

    // Provider windows read/mutate m_ProviderStore; hold the registry lock so
    // they don't race the executor's ConfigureProvider.
    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        chess::application::RenderModelProviderConfigurationWindow(&s_ShowProviderConfigWindow, m_ProviderStore);
        chess::application::RenderProvidersWindow(&s_ShowProvidersWindow, m_ProviderStore);
    }

    if (s_ShowFENWindow) {
        ImGui::Begin("FEN", &s_ShowFENWindow);

        m_BoardFEN.resize(256);
        bool entered = ImGui::InputText("##FEN", m_BoardFEN.data(), m_BoardFEN.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        m_BoardFEN.resize(strlen(m_BoardFEN.data()));

        if (entered) {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            m_Board->FromFEN(m_BoardFEN);
        }

        if (ImGui::Button("Copy FEN to clipboard"))
            SDL_SetClipboardText(m_BoardFEN.c_str());

        if (ImGui::Button("Reset board")) {
            {
                std::lock_guard<std::mutex> lock(*m_BoardMutex);
                m_Board->Reset();  // Reset FEN string
                m_BoardFEN = m_Board->ToFEN();
            }
            if (m_RunningEngine) {
                m_RunningEngine->SetPosition(m_BoardFEN);
            }
        }

        ImGui::End();
    }

    // Render any open spectator views and prune those the user has closed.
    for (const auto& view : m_GameViews) {
        view->Render();
    }
    std::erase_if(m_GameViews, [](const std::unique_ptr<chess::application::GameView>& view) {
        return !view->IsOpen();
    });
}


std::shared_ptr<chess::game::GameContext> Application::CreateGame(const chess::game::GameConfiguration& config, const std::string& game_id) {
    // Resolve the per-color provider credentials from the configured providers.
    chess::game::GameConfiguration resolved = config;
    resolved.white.api_key = ResolveApiKey(resolved.white.provider);
    resolved.black.api_key = ResolveApiKey(resolved.black.provider);

    auto context = std::make_shared<chess::game::GameContext>();
    context->configuration = resolved;
    context->board = std::make_shared<Board>();
    context->board_mutex = std::make_shared<std::mutex>();
    context->phase = std::make_shared<std::atomic<chess::game::GameLifecyclePhase>>(
        chess::game::GameLifecyclePhase::Initial
    );
    context->result = std::make_shared<chess::game::execution::MatchResult>();
    context->move_log = std::make_shared<chess::game::GameMoveLog>();
    context->white_trajectory = std::make_shared<AgentTrajectory>();
    context->black_trajectory = std::make_shared<AgentTrajectory>();
    context->white_usage_history = std::make_shared<tbb::concurrent_vector<chess::agent::message::ModelUsage>>();
    context->black_usage_history = std::make_shared<tbb::concurrent_vector<chess::agent::message::ModelUsage>>();
    context->white_model_pricing = std::make_shared<chess::agent::message::ModelPricing>();
    context->black_model_pricing = std::make_shared<chess::agent::message::ModelPricing>();

    so_5::environment_t& env = m_GameOrchestrationEnvironment->environment();

    // Create Gaunt game context from session if telemetry is initialized
    std::shared_ptr<chess::telemetry::GauntGameContext> gaunt_game_ctx;
    if (m_GauntContextHandles && m_GauntContextHandles->session_context.has_value()) {
        gaunt_game_ctx = std::make_shared<chess::telemetry::GauntGameContext>();

        // Use monotonically increasing game counter for key
        static std::atomic<uint32_t> s_GameCounter{0};
        const std::string game_key = std::to_string(++s_GameCounter);

        gaunt_game_ctx->game_context = m_GauntContextHandles->session_context->CreateChildContext<
            gaunt::generated::chess_coliseum::context::Game>(
            gaunt::generated::chess_coliseum::context::Game::Key{game_key},
            gaunt::generated::chess_coliseum::event::GameBegin{}
        );
    }

    chess::game::execution::GameOrchestrator* orchestrator = nullptr;
    env.introduce_coop([&](so_5::coop_t& coop) {
        chess::game::execution::GameOrchestratorState state;
        state.game_configuration = resolved;
        state.game_state.board = context->board;            // share the source-of-truth board
        state.game_state.board_mutex = context->board_mutex; // shared guard for players/views
        state.game_id = game_id;                            // empty -> orchestrator self-generates
        state.published_phase = context->phase;             // let the actor publish its phase
        state.published_result = context->result;           // final result sink (read post-conclude)
        state.move_log = context->move_log;                 // per-move outcomes (views read)
        state.white_trajectory = context->white_trajectory; // streamed agent events -> views
        state.black_trajectory = context->black_trajectory;
        state.white_usage_history = context->white_usage_history; // per-turn token usage (cost estimate)
        state.black_usage_history = context->black_usage_history;
        state.white_model_pricing = context->white_model_pricing; // per-token pricing (cost estimate)
        state.black_model_pricing = context->black_model_pricing;
        state.gaunt_context = gaunt_game_ctx;               // telemetry context for this game
        orchestrator = coop.make_agent<chess::game::execution::GameOrchestrator>(std::move(state));
    });

    // The coop is registered now, so the agent's id and direct mbox are valid.
    context->id = orchestrator->GetGameId();
    context->command_mbox = orchestrator->so_direct_mbox();

    // Allocate the game's audio buses now (named per process + game) but defer
    // opening the MIX devices to the main thread (see ProcessPendingAudioInit) —
    // CreateGame may run on the command-executor thread.
#ifdef _WIN32
    const long pid = static_cast<long>(_getpid());
#else
    const long pid = static_cast<long>(getpid());
#endif
    context->audio = std::make_shared<chess::game::GameAudio>();
    context->audio->sfx_name = "Chess(" + std::to_string(pid) + ").Game(" + context->id + ").SoundEffects";
    context->audio->music_name = "Chess(" + std::to_string(pid) + ").Game(" + context->id + ").Music";

    // Drive the lifecycle: configure (with resolved credentials), then begin
    // player setup. The orchestrator processes these in order and progresses to
    // 'ready' once setup completes.
    so_5::send<chess::game::GameConfiguration>(context->command_mbox, resolved);
    so_5::send<chess::game::execution::BeginSetupRequest>(context->command_mbox);

    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        m_Games[context->id] = context;
        m_PendingAudioInit.push_back(context); // main thread opens its MIX devices
    }
    return context;
}

// Per-frame, main-thread audio step (MIX device creation must be on the main
// thread). Opens MIX devices + loads SFX for newly-created games, then plays the
// capture sound whenever a game's captured-piece count grows.
void Application::UpdateGameAudio() {
    // Always drain the pending queue so it can't grow unbounded; only actually
    // open devices when audio is available.
    std::vector<std::shared_ptr<chess::game::GameContext>> pending;
    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        pending.swap(m_PendingAudioInit);
    }
    if (!m_AudioAvailable) {
        return;
    }

    for (const auto& context : pending) {
        if (!context || !context->audio) {
            continue;
        }
        chess::game::GameAudio& audio = *context->audio;
        if (!audio.sfx_mixer) {
            audio.sfx_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (audio.sfx_mixer) {
                MIX_SetMixerGain(audio.sfx_mixer, audio.sfx_gain);
                // Bake the capture SFX into this game's SFX mixer.
                if (SDL_IOStream* io = SDL_IOFromConstMem(
                        chess::resources::sound::kConfirm01WavBytes,
                        sizeof(chess::resources::sound::kConfirm01WavBytes))) {
                    audio.capture_sfx = MIX_LoadAudio_IO(audio.sfx_mixer, io, /*predecode=*/true, /*closeio=*/true);
                }
                // Queen capture sound.
                if (SDL_IOStream* io = SDL_IOFromConstMem(
                        chess::resources::sound::kCollectPoint2WavBytes,
                        sizeof(chess::resources::sound::kCollectPoint2WavBytes))) {
                    audio.queen_capture_sfx = MIX_LoadAudio_IO(audio.sfx_mixer, io, /*predecode=*/true, /*closeio=*/true);
                }
                // Check sound.
                if (SDL_IOStream* io = SDL_IOFromConstMem(
                        chess::resources::sound::kFireHit01WavBytes,
                        sizeof(chess::resources::sound::kFireHit01WavBytes))) {
                    audio.check_sfx = MIX_LoadAudio_IO(audio.sfx_mixer, io, /*predecode=*/true, /*closeio=*/true);
                }
                // Checkmate sound.
                if (SDL_IOStream* io = SDL_IOFromConstMem(
                        chess::resources::sound::kActivatePowerWavBytes,
                        sizeof(chess::resources::sound::kActivatePowerWavBytes))) {
                    audio.checkmate_sfx = MIX_LoadAudio_IO(audio.sfx_mixer, io, /*predecode=*/true, /*closeio=*/true);
                }
                // Error sound (invalid move, agent failure).
                if (SDL_IOStream* io = SDL_IOFromConstMem(
                        chess::resources::sound::kNoEntryWavBytes,
                        sizeof(chess::resources::sound::kNoEntryWavBytes))) {
                    audio.error_sfx = MIX_LoadAudio_IO(audio.sfx_mixer, io, /*predecode=*/true, /*closeio=*/true);
                }
            }
        }
        if (!audio.music_mixer) {
            audio.music_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (audio.music_mixer) {
                MIX_SetMixerGain(audio.music_mixer, audio.music_gain);
            }
        }
    }

    // Play sound effects for game events. Snapshot the games under the registry
    // lock, then process outside it.
    std::vector<std::shared_ptr<chess::game::GameContext>> games;
    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        games.reserve(m_Games.size());
        for (const auto& [id, context] : m_Games) {
            games.push_back(context);
        }
    }
    for (const auto& context : games) {
        if (!context || !context->audio || !context->audio->sfx_mixer) {
            continue;
        }
        chess::game::GameAudio& audio = *context->audio;

        // Gather current game state.
        std::vector<Piece> all_captures;
        if (context->white_trajectory) {
            auto white_caps = context->white_trajectory->GetCapturedPieces();
            all_captures.insert(all_captures.end(), white_caps.begin(), white_caps.end());
        }
        if (context->black_trajectory) {
            auto black_caps = context->black_trajectory->GetCapturedPieces();
            all_captures.insert(all_captures.end(), black_caps.begin(), black_caps.end());
        }

        std::size_t queen_captures = 0;
        for (const Piece p : all_captures) {
            if (p == WhiteQueen || p == BlackQueen) {
                ++queen_captures;
            }
        }

        std::vector<chess::game::MoveOutcome> all_outcomes;
        if (context->move_log) {
            all_outcomes = context->move_log->AllOutcomes();
        }

        auto& state = m_GameAudioStates[context->id];

        // Seed on first encounter; don't replay history.
        if (state.capture_count == 0 && state.queen_capture_count == 0 && state.total_moves_count == 0) {
            state.capture_count = all_captures.size();
            state.queen_capture_count = queen_captures;
            state.total_moves_count = all_outcomes.size();
            continue;
        }

        // Process new move outcomes with priority system.
        // For each new move, determine the highest priority sound to play.
        // Priority (highest to lowest): Checkmate > Queen capture > Check > Regular capture > Error
        if (all_outcomes.size() > state.total_moves_count) {
            for (std::size_t i = state.total_moves_count; i < all_outcomes.size(); ++i) {
                const auto& outcome = all_outcomes[i];

                if (!outcome.accepted) {
                    // Rejected move: play error sound.
                    if (audio.error_sfx) {
                        MIX_PlayAudio(audio.sfx_mixer, audio.error_sfx);
                    }
                    continue;
                }

                // Accepted move: determine highest priority sound.
                // Check for checkmate first (highest priority).
                const bool is_checkmate = !outcome.long_algebraic_notation.empty() && outcome.long_algebraic_notation.back() == '#';
                const bool is_check = !outcome.long_algebraic_notation.empty() && outcome.long_algebraic_notation.back() == '+';
                const bool is_queen_capture = queen_captures > state.queen_capture_count;
                const bool is_capture = all_captures.size() > state.capture_count;

                if (is_checkmate && audio.checkmate_sfx) {
                    MIX_PlayAudio(audio.sfx_mixer, audio.checkmate_sfx);
                } else if (is_queen_capture && audio.queen_capture_sfx) {
                    MIX_PlayAudio(audio.sfx_mixer, audio.queen_capture_sfx);
                } else if (is_check && audio.check_sfx) {
                    MIX_PlayAudio(audio.sfx_mixer, audio.check_sfx);
                } else if (is_capture && audio.capture_sfx) {
                    MIX_PlayAudio(audio.sfx_mixer, audio.capture_sfx);
                }

                // Update capture tracking for next iteration.
                state.queen_capture_count = queen_captures;
                state.capture_count = all_captures.size();
            }
            state.total_moves_count = all_outcomes.size();
        }
    }
}

void Application::ConfigureProvider(const chess::application::provider::ConfiguredProvider& provider) {
    std::lock_guard<std::mutex> lock(m_RegistryMutex);
    m_ProviderStore.providers.push_back(provider);
}

std::string Application::ResolveApiKey(chess::game::AgentProvider provider) {
    // Only OpenRouter credentials are wired up for now; return the first
    // matching configured provider's key.
    if (provider != chess::game::AgentProvider::OpenRouter) {
        return "";
    }
    std::lock_guard<std::mutex> lock(m_RegistryMutex);
    for (const auto& configured : m_ProviderStore.providers) {
        if (const auto* open_router = std::get_if<chess::application::provider::OpenRouterModelProviderConfiguration>(&configured.configuration)) {
            return open_router->api_key;
        }
    }
    return "";
}

std::shared_ptr<chess::game::GameContext> Application::FindGame(const std::string& game_id) {
    std::lock_guard<std::mutex> lock(m_RegistryMutex);
    const auto it = m_Games.find(game_id);
    return it == m_Games.end() ? nullptr : it->second;
}

SpectatorViewResult Application::OpenSpectatorView(
    const std::string& game_id,
    chess::application::GameViewTheme theme,
    std::shared_ptr<chess::application::command::WindowConfiguration> window_config
) {
    const auto context = FindGame(game_id);
    if (!context) {
        return SpectatorViewResult{.success = false};
    }

    ++m_SpectatorViewCounter;
    std::string title = "Spectator " + std::to_string(m_SpectatorViewCounter) + " - " + game_id;
    auto view = std::make_unique<chess::application::GameView>(
        context, theme, m_HeaderFont, title
    );

    // Apply initial window configuration if provided.
    if (window_config) {
        view->SetInitialWindowConfig(std::move(window_config));
    }

    m_GameViews.push_back(std::move(view));

    // Retrieve OS-level window metadata for programmatic screen capture.
    // Note: In multi-viewport mode, ImGui windows that pop out become their own
    // SDL windows. However, the window isn't created until after ImGui renders
    // and UpdatePlatformWindows() is called. For now, we return the main window's
    // metadata; the caller can use the ImGui window ID for more advanced capture.
    chess::application::command::OSWindowMetadata os_metadata;
#ifdef __linux__
    os_metadata = chess::platform::platform_linux::GetWindowMetadata(m_Window);
#endif

    return SpectatorViewResult{
        .success = true,
        .window_title = title,
        .os_metadata = os_metadata
    };
}

void Application::ProcessPendingSpectatorViews() {
    // Drain the pending queue under lock, then process outside the lock.
    std::vector<PendingSpectatorView> pending;
    {
        std::lock_guard<std::mutex> lock(m_RegistryMutex);
        while (!m_PendingSpectatorViews.empty()) {
            pending.push_back(std::move(m_PendingSpectatorViews.front()));
            m_PendingSpectatorViews.pop();
        }
    }

    for (auto& entry : pending) {
        // Parse the theme from the command.
        chess::application::GameViewTheme theme = chess::application::GameViewTheme::Default;
        if (entry.command.game_view_theme == "high_contrast") {
            theme = chess::application::GameViewTheme::HighContrast;
        }

        // Create window configuration.
        auto window_config = std::make_shared<chess::application::command::WindowConfiguration>(
            entry.command.window_configuration);

        // Create the spectator view on the UI thread.
        SpectatorViewResult result = OpenSpectatorView(entry.command.game_id, theme, window_config);

        if (!result.success) {
            // Game not found - respond immediately with error.
            if (entry.callback) {
                chess::application::command::OpenSpectatorViewResponse response;
                response.code = 404;
                entry.callback(std::move(response));
            }
            continue;
        }

        // Determine if this is a floating window (needs deferred metadata collection).
        const bool is_floating = window_config->type == "floating";

        if (is_floating) {
            // Floating window: defer callback until after UpdatePlatformWindows()
            // when the viewport exists and we can get the native window handle.
            m_DeferredSpectatorViewMetadata.push_back(DeferredSpectatorViewMetadata{
                .window_title = result.window_title,
                .callback = std::move(entry.callback)
            });
        } else {
            // Docked window: shares main window's OS handle, respond immediately.
            if (entry.callback) {
                chess::application::command::OpenSpectatorViewResponse response;
                response.code = 200;
                response.window_metadata.imgui_metadata.window_id = result.window_title;
                response.window_metadata.os_metadata = result.os_metadata;
                entry.callback(std::move(response));
            }
        }
    }
}

void Application::CollectDeferredSpectatorViewMetadata() {
    if (m_DeferredSpectatorViewMetadata.empty()) {
        return;
    }

    // Process all deferred entries - the viewports should now exist.
    std::vector<DeferredSpectatorViewMetadata> deferred;
    deferred.swap(m_DeferredSpectatorViewMetadata);

    for (auto& entry : deferred) {
        chess::application::command::OpenSpectatorViewResponse response;
        response.code = 200;
        response.window_metadata.imgui_metadata.window_id = entry.window_title;

        // Find the ImGui window by name and get its viewport's native handle.
        ImGuiWindow* window = ImGui::FindWindowByName(entry.window_title.c_str());
        if (window && window->Viewport && window->Viewport->PlatformHandle) {
            SDL_Window* sdl_window = static_cast<SDL_Window*>(window->Viewport->PlatformHandle);
#ifdef __linux__
            response.window_metadata.os_metadata = chess::platform::platform_linux::GetWindowMetadata(sdl_window);
#else
            (void)sdl_window;
#endif
        } else {
            // Viewport not ready yet - fall back to main window metadata.
#ifdef __linux__
            response.window_metadata.os_metadata = chess::platform::platform_linux::GetWindowMetadata(m_Window);
#endif
        }

        if (entry.callback) {
            entry.callback(std::move(response));
        }
    }
}

void Application::OnWindowClose() {
    m_Running = false;
}

void Application::OnWindowResize(int32_t width, int32_t height) {
    m_WindowProperties.Width = width;
    m_WindowProperties.Height = height;
}

void Application::OnKeyPressed(SDL_Keycode key) {
    if (key == SDLK_ESCAPE) {
        m_Running = false;
    }
}

void Application::OnMouseButton(const SDL_MouseButtonEvent& event) {
    // Logic moved to CentralChessboardPanel::Render
}
