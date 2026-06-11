#include "Application.h"
#include "Resources.h"
#include "Chess/Board.h"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <iostream>

Application* Application::s_Instance = nullptr;

Application::Application(uint32_t width, uint32_t height, const std::string& name, const ProgramArgs& args)
    : m_WindowProperties{ width, height, name },
        m_ChessViewportSize{static_cast<float>(width), static_cast<float>(height)},
        m_Args(args),
        m_Board(std::make_shared<Board>()),
        m_BoardMutex(std::make_shared<std::mutex>()) {

    if (!s_Instance) {
        s_Instance = this;
    }

    // SDL_Init replaces glfwInit. Required to initialize the video subsystem.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cout << "Could not initialize SDL: " << SDL_GetError() << "\n";
        return;
    }

    // SDL_CreateWindow replaces glfwCreateWindow.
    m_Window = SDL_CreateWindow(name.c_str(), 966, 600,  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_Window) {
        std::cout << "Could not create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return;
    }

    // SDL_Renderer replaces the OpenGL context. Required for 2D hardware accelerated rendering.
    SDL_Renderer* renderer = SDL_CreateRenderer(m_Window, nullptr);
    if (!renderer) {
        std::cout << "Could not create renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(m_Window);
        SDL_Quit();
        return;
    }
    m_Renderer = std::shared_ptr<SDL_Renderer>(renderer, SDL_DestroyRenderer);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Platform/Renderer backends. We use SDL3 + SDL_Renderer backends.
    ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer.get());
    ImGui_ImplSDLRenderer3_Init(m_Renderer.get());
}

Application::~Application() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_BoardTargetTexture)
        SDL_DestroyTexture(m_BoardTargetTexture);

    m_Renderer.reset();

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

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        
        {
            std::lock_guard<std::mutex> lock(*m_BoardMutex);
            m_BoardFEN = m_Board->ToFEN();
        }

        RenderImGui();

        ImGui::Render();

        // Clear with background colour and render ImGui draw data.
        SDL_SetRenderDrawColor(m_Renderer.get(), m_BackgroundColour.r, m_BackgroundColour.g, m_BackgroundColour.b, m_BackgroundColour.a);
        SDL_RenderClear(m_Renderer.get());
        
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer.get());

        // SDL_RenderPresent replaces glfwSwapBuffers.
        SDL_RenderPresent(m_Renderer.get());
    }
}

TextureView Application::GetChessSprite(Piece p) {
    // Uses the new PaletteSwappedPieceAtlas accessors
    switch (p) {
    case WhitePawn:   return m_TextureResources.white_pieces->GetPawnTexture();
    case WhiteKnight: return m_TextureResources.white_pieces->GetKnightTexture();
    case WhiteBishop: return m_TextureResources.white_pieces->GetBishopTexture();
    case WhiteRook:   return m_TextureResources.white_pieces->GetRookTexture();
    case WhiteQueen:  return m_TextureResources.white_pieces->GetQueenTexture();
    case WhiteKing:   return m_TextureResources.white_pieces->GetKingTexture();
    case BlackPawn:   return m_TextureResources.black_pieces->GetPawnTexture();
    case BlackKnight: return m_TextureResources.black_pieces->GetKnightTexture();
    case BlackBishop: return m_TextureResources.black_pieces->GetBishopTexture();
    case BlackRook:   return m_TextureResources.black_pieces->GetRookTexture();
    case BlackQueen:  return m_TextureResources.black_pieces->GetQueenTexture();
    case BlackKing:   return m_TextureResources.black_pieces->GetKingTexture();
    case None:        return { nullptr, {0,0,0,0} };
    }

    throw std::runtime_error("Invalid Piece enum!");
}

void Application::Init() {
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;

    void* font = (void*)Resources::Fonts::Roboto::ROBOTO_REGULAR;
    int32_t fontSize = sizeof(Resources::Fonts::Roboto::ROBOTO_REGULAR);
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(font, fontSize, 20.0f, &fontConfig);

    if (!std::filesystem::exists("imgui.ini"))
        ImGui::LoadIniSettingsFromMemory(Resources::DEFAULT_IMGUI_INI);

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

    // Instantiate our palette-swapped resources.
    m_TextureResources.white_pieces = std::make_shared<PaletteSwappedPieceAtlas>(kBasicWhiteColorPalette, m_Renderer);
    m_TextureResources.black_pieces = std::make_shared<PaletteSwappedPieceAtlas>(kBasicBlackColorPalette, m_Renderer);
    m_TextureResources.board = std::make_shared<PaletteSwappedBoard>(kBasicBoardColorPalette, m_Renderer);

    m_LegalMoveColour = { 255, 0, 255, 127 };
    m_BackgroundColour = { 51, 51, 51, 255 };

    m_BoardFEN = m_Board->ToFEN();

    // Create the target texture for the chess board viewport.
    m_BoardTargetTexture = SDL_CreateTexture(m_Renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, m_WindowProperties.Width, m_WindowProperties.Height);

    if (!m_Args.whiteEndpoint.empty()) {
        m_WhiteAgent = std::make_shared<ZMQAgentServer>(m_Args.whiteEndpoint, "WHITE");
        m_WhiteAgent->Start();
        m_WhiteSidebar.SetTrajectory(m_WhiteAgent->GetTrajectory());
    }

    if (!m_Args.blackEndpoint.empty()) {
        m_BlackAgent = std::make_shared<ZMQAgentServer>(m_Args.blackEndpoint, "BLACK");
        m_BlackAgent->Start();
        m_BlackSidebar.SetTrajectory(m_BlackAgent->GetTrajectory());
    }

    m_Orchestrator = std::make_shared<GameOrchestrator>(m_WhiteAgent, m_BlackAgent, m_Board, m_BoardMutex, m_Args.retrospectiveRounds);
    m_Orchestrator->Start();
}

void Application::RenderImGui() {
    static bool s_ShowSettingsWindow = false, s_ShowFENWindow = false, s_ShowEngineWindow = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New");
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                m_Running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
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
            ImGui::Text("SDL3 Backend");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
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
            if (m_RunningEngine)
                m_RunningEngine->SetPosition(m_BoardFEN);
            m_WhiteSidebar.ClearChat();
            m_BlackSidebar.ClearChat();
        }

        ImGui::End();
    }

    // RenderChessPanel();
    // RenderSettingsPanel(&s_ShowSettingsWindow);
    // RenderEnginePanel(&s_ShowEngineWindow);
    m_WhiteSidebar.Render();
    m_BlackSidebar.Render();
}

void Application::OnWindowClose() {
    m_Running = false;
}

void Application::OnWindowResize(int32_t width, int32_t height) {
    m_WindowProperties.Width = width;
    m_WindowProperties.Height = height;

    // Recreate the target texture on resize to match window dimensions.
    if (m_BoardTargetTexture)
        SDL_DestroyTexture(m_BoardTargetTexture);
    m_BoardTargetTexture = SDL_CreateTexture(m_Renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
}

void Application::OnKeyPressed(SDL_Keycode key) {
    if (key == SDLK_ESCAPE) {
        m_Running = false;
    }
}

void Application::OnMouseButton(const SDL_MouseButtonEvent& event) {
    std::lock_guard<std::mutex> lock(*m_BoardMutex);
    if (event.button == SDL_BUTTON_LEFT) {
        // Use the board mouse position calculated in ChessPanel.cpp
        SDL_FPoint& point = m_BoardMousePosition;

        if (event.down) {
            m_IsHoldingPiece = true;

            if (point.x > -4 && point.x < 4 && point.y > -4 && point.y < 4) {
                Square rank = (Square)(point.x + 4.0f);
                Square file = (Square)(point.y + 4.0f);

                // The square the mouse clicked on
                Square selectedSquare = ToSquare('a' + rank, '1' + file);

                // If a piece was already selected, move piece to clicked square
                if (m_SelectedPiece != INVALID_SQUARE && m_SelectedPiece != selectedSquare) {
                    if (m_LegalMoves & (1ull << selectedSquare) || selectedSquare == m_SelectedPiece) {
                        m_Board->Move({ m_SelectedPiece, selectedSquare });
                        m_BoardFEN = m_Board->ToFEN();
                        if (m_RunningEngine)
                            m_RunningEngine->SetPosition(m_BoardFEN);
                    }

                    m_SelectedPiece = INVALID_SQUARE;
                    m_LegalMoves = 0;
                }
                else {  // If no piece already selected, select piece
                    m_LegalMoves = m_Board->GetPieceLegalMoves(selectedSquare);
                    m_SelectedPiece = m_LegalMoves == 0 ? INVALID_SQUARE : selectedSquare;
                }
            }
            else {
                m_SelectedPiece = INVALID_SQUARE;
                m_LegalMoves = 0;
            }
        }
        else { // SDL_EVENT_MOUSE_BUTTON_UP
            if (point.x > -4 && point.x < 4 && point.y > -4 && point.y < 4) {
                Square rank = (Square)(point.x + 4.0f);
                Square file = (Square)(point.y + 4.0f);

                // The square the mouse was released on
                Square selectedSquare = ToSquare('a' + rank, '1' + file);

                if (m_SelectedPiece != INVALID_SQUARE) {
                    if (m_LegalMoves & (1ull << selectedSquare)) {
                        m_Board->Move({ m_SelectedPiece, selectedSquare });
                        m_BoardFEN = m_Board->ToFEN();
                        if (m_RunningEngine)
                            m_RunningEngine->SetPosition(m_BoardFEN);
                        m_LegalMoves = 0;
                    }
                }
            }

            m_IsHoldingPiece = false;
            m_SelectedPiece = INVALID_SQUARE;
        }
    }
    else if (event.button == SDL_BUTTON_RIGHT) {
        m_SelectedPiece = INVALID_SQUARE;
        m_IsHoldingPiece = false;
        m_LegalMoves = 0;
    }
}
