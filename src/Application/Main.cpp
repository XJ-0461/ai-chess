#include "Application.h"

#ifdef _WIN32

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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
        MessageBox(glfwGetWin32Window(app->GetGLFWWindow()), e.what(), NULL, MB_OK | MB_ICONERROR);
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
