#include "Application.h"

#include "Graphics/Pieces/PieceAtlas.hpp"

#ifdef _WIN32

#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    ProgramArgs args;
    auto app = new Application(1600, 900, "Chess", args);
    try {
        app->Run();
    }
    catch (std::exception& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Chess", e.what(), app->GetWindow());
    }
    delete app;

    return 0;
}

#else

#include <iostream>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    ProgramArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--white-endpoint" && i + 1 < argc) {
            args.whiteEndpoint = argv[++i];
        } else if (arg == "--black-endpoint" && i + 1 < argc) {
            args.blackEndpoint = argv[++i];
        } else if (arg == "--retrospective-rounds" && i + 1 < argc) {
            args.retrospectiveRounds = std::stoul(argv[++i]);
        } else if (arg == "--commands" && i + 1 < argc) {
            args.commandFilePath = argv[++i];
        } else if (arg == "--command-server-endpoint" && i + 1 < argc) {
            args.commandServerEndpoint = argv[++i];
        } else if (arg == "--gaunt-telemetry-xml-output-file" && i + 1 < argc) {
            args.gauntTelemetryXmlOutputFile = argv[++i];
        }
    }

    auto app = new Application(1280, 720, "Chess", args);
    try {
        app->Run();
    }
    catch (std::exception& e) {
        std::cout << "Unhandled exception: " << e.what() << "\n";
        std::cin.get();
    }
    delete app;
}

#endif
