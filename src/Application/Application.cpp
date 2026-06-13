
#include <iostream>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "Application.h"
#include "Resources.h"
#include "Chess/Board.h"
#include "Graphics/Font/PixelOperatorSCBold.hpp"
#include "Graphics/Icon/CheckDoubleSVG.hpp"
#include "Graphics/Icon/WarningSVG.hpp"
#include "Graphics/SVG/Import.hpp"
#include "Graphics/Theme/Color/DarkAgentSidebarColorPalette.hpp"
#include "Graphics/Theme/Color/WoodAgentSidebarColorPalette.hpp"

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

        if (m_WhitePaletteNeedsUpdate) {
            m_WhitePaletteNeedsUpdate = false;
            auto details = PaletteDetailsFromString(m_WhiteModelName);
            if (details) {
                UpdatePlayerColorPalette(White, details->white_palette);
            }
        }

        if (m_BlackPaletteNeedsUpdate) {
            m_BlackPaletteNeedsUpdate = false;
            auto details = PaletteDetailsFromString(m_BlackModelName);
            if (details) {
                UpdatePlayerColorPalette(Black, details->black_palette);
            }
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

void Application::UpdatePlayerColorPalette(const Colour piece_color, PieceColorPaletteT palette) {
    if (piece_color == White) {
        m_TextureResources.white_pieces = std::make_shared<PaletteSwappedPieceAtlas>(palette, m_Renderer);
        m_TextureResources.white_pieces->MakeTexture();
        m_WhiteSidebar.SetPieceAtlas(m_TextureResources.white_pieces);
        m_WhiteSidebar.SetColorPalette(
            MergeWithPiecePalette(m_AgentSidebarColorPalette, palette)
        );
    } else {
        m_TextureResources.black_pieces = std::make_shared<PaletteSwappedPieceAtlas>(palette, m_Renderer);
        m_TextureResources.black_pieces->MakeTexture();
        m_BlackSidebar.SetPieceAtlas(m_TextureResources.black_pieces);
        m_BlackSidebar.SetColorPalette(
            MergeWithPiecePalette(m_AgentSidebarColorPalette, palette)
        );
    }
    m_CentralPanel.SetPieceAtlases(m_TextureResources.white_pieces, m_TextureResources.black_pieces);
}

AgentSidebarColorPalette Application::MergeWithPiecePalette(
    const AgentSidebarColorPalette& current_palette,
    const PieceColorPaletteT& piece_palette
) {
    AgentSidebarColorPalette merged = current_palette;

    constexpr auto to_imvec4 = [](const RGBA& rgba) {
        return ImVec4(rgba.red / 255.0f, rgba.green / 255.0f, rgba.blue / 255.0f, rgba.alpha / 255.0f);
    };

    merged.profile_background = to_imvec4(piece_palette[0]);
    merged.profile_border = to_imvec4(piece_palette[1]);

    return merged;
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

    m_HeaderFont = io.Fonts->AddFontFromMemoryTTF((void*)kPixelOperatorSCBoldTFFBytes, sizeof(kPixelOperatorSCBoldTFFBytes), 24.0f, &fontConfig);
    m_WhiteSidebar.SetHeaderFont(m_HeaderFont);
    m_BlackSidebar.SetHeaderFont(m_HeaderFont);

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
    m_TextureResources.white_pieces->MakeTexture();
    m_TextureResources.black_pieces = std::make_shared<PaletteSwappedPieceAtlas>(kBasicBlackColorPalette, m_Renderer);
    m_TextureResources.black_pieces->MakeTexture();
    m_TextureResources.board = std::make_shared<PaletteSwappedBoardAtlas>(kWoodBoardColorPalette, m_Renderer);
    m_AgentSidebarColorPalette = chess::style::color::kWoodAgentSidebarColorPalette;

    m_WhiteSidebar.SetColorPalette(
        MergeWithPiecePalette(m_AgentSidebarColorPalette, kBasicWhiteColorPalette)
    );
    m_BlackSidebar.SetColorPalette(
        MergeWithPiecePalette(m_AgentSidebarColorPalette, kBasicBlackColorPalette)
    );


    m_WhiteSidebar.SetPieceAtlas(m_TextureResources.white_pieces);
    m_BlackSidebar.SetPieceAtlas(m_TextureResources.black_pieces);

    m_CentralPanel.SetPieceAtlases(m_TextureResources.white_pieces, m_TextureResources.black_pieces);
    m_CentralPanel.SetBoardAtlas(m_TextureResources.board);

    m_TextureResources.double_check_icon = chess::graphics::svg::LoadSVGTextureFromMemory(m_Renderer, kCheckDoubleSvg);
    m_TextureResources.warning_icon = chess::graphics::svg::LoadSVGTextureFromMemory(m_Renderer, kWarningSvg);

    m_CentralPanel.SetGameState(m_Board, m_BoardMutex);
    m_CentralPanel.SetInteractionState(&m_SelectedPiece, &m_LegalMoves, &m_IsHoldingPiece);

    m_LegalMoveColour = { 255, 0, 255, 127 };
    m_BackgroundColour = { 51, 51, 51, 255 };

    m_BoardFEN = m_Board->ToFEN();

    // Create the target texture for the chess board viewport.
    m_BoardTargetTexture = SDL_CreateTexture(m_Renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, m_WindowProperties.Width, m_WindowProperties.Height);

    if (!m_Args.whiteEndpoint.empty()) {
        m_WhiteAgent = std::make_shared<ZMQAgentServer>(m_Args.whiteEndpoint, "WHITE");
        m_WhiteAgent->Start();
        m_WhiteSidebar.SetTrajectory(m_WhiteAgent->GetTrajectory());
        m_WhiteAgent->GetTrajectory()->SetOnModelUpdateCallback([this](const std::string& name) {
            m_WhiteModelName = name;
            m_WhitePaletteNeedsUpdate = true;
        });
    }

    if (!m_Args.blackEndpoint.empty()) {
        m_BlackAgent = std::make_shared<ZMQAgentServer>(m_Args.blackEndpoint, "BLACK");
        m_BlackAgent->Start();
        m_BlackSidebar.SetTrajectory(m_BlackAgent->GetTrajectory());
        m_BlackAgent->GetTrajectory()->SetOnModelUpdateCallback([this](const std::string& name) {
            m_BlackModelName = name;
            m_BlackPaletteNeedsUpdate = true;
        });
    }

    m_Orchestrator = std::make_shared<GameOrchestrator>(m_WhiteAgent, m_BlackAgent, m_Board, m_BoardMutex, m_Args.retrospectiveRounds);
    m_Orchestrator->Start();
}

void Application::RenderImGui() {
    static bool s_ShowSettingsWindow = false, s_ShowFENWindow = false, s_ShowEngineWindow = false;

    // if (ImGui::BeginMainMenuBar()) {
    //     if (ImGui::BeginMenu("File")) {
    //         ImGui::MenuItem("New");
    //         ImGui::Separator();
    //         if (ImGui::MenuItem("Quit")) {
    //             m_Running = false;
    //         }
    //         ImGui::EndMenu();
    //     }
    //     if (ImGui::BeginMenu("View")) {
    //         if (ImGui::MenuItem("Colours")) {
    //             s_ShowSettingsWindow = true;
    //         }
    //         if (ImGui::MenuItem("FEN")) {
    //             s_ShowFENWindow     = true;
    //         }
    //         if (ImGui::MenuItem("Engine")) {
    //             s_ShowEngineWindow  = true;
    //         }
    //         ImGui::EndMenu();
    //     }
    //     if (ImGui::BeginMenu("About")) {
    //         ImGui::Text("SDL3 Backend");
    //         ImGui::EndMenu();
    //     }
    //     ImGui::EndMainMenuBar();
    // }

    m_Layout.Render();

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
    // Logic moved to CentralChessboardPanel::Render
}
