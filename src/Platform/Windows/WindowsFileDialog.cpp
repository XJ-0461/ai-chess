#include "Utility/FileDialog.h"

#include <Windows.h>

#include <SDL3/SDL.h>

#include "Application/Application.h"

namespace FileDialog {

	namespace {
		// Retrieve the native Win32 window handle backing the SDL window.
		HWND NativeWindowHandle() {
			SDL_Window* const window = Application::Get().GetWindow();
			return static_cast<HWND>(SDL_GetPointerProperty(
				SDL_GetWindowProperties(window),
				SDL_PROP_WINDOW_WIN32_HWND_POINTER,
				nullptr
			));
		}
	}

	std::string Open() {
		OPENFILENAMEA ofn;
		CHAR szFile[255] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(Application::Get().GetGLFWWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		// User cancelled the dialog
		if (CommDlgExtendedError() == 0)
			return "";

		// Error
		throw std::runtime_error("Error with open file dialog!");
	}

	std::string Open(char* buffer, size_t size) {
		OPENFILENAMEA ofn;
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(Application::Get().GetGLFWWindow());
		ofn.lpstrFile = buffer;
		ofn.nMaxFile = (DWORD)size;
		ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		// User cancelled the dialog
		if (CommDlgExtendedError() == 0)
			return "";

		// Error
		throw std::runtime_error("Error with open file dialog!");
	}

	std::string Save() {
		OPENFILENAMEA ofn;
		CHAR szFile[255] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(Application::Get().GetGLFWWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		// User cancelled the dialog
		if (CommDlgExtendedError() == 0)
			return "";

		// Error
		throw std::runtime_error("Error with open file dialog!");
	}

}
