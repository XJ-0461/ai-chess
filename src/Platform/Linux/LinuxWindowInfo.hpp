#pragma once

#include <SDL3/SDL.h>

#include "Application/Command/CommandResponse.hpp"

namespace chess::platform::platform_linux {

// Retrieves OS-level window metadata (X11 Window ID or Wayland surface) for
// programmatic screen capture. Returns monostate if the display server type
// is unsupported or metadata retrieval fails.
[[nodiscard]]
inline chess::application::command::OSWindowMetadata GetWindowMetadata(SDL_Window* window) {
    if (!window) {
        return std::monostate{};
    }

    const char* video_driver = SDL_GetCurrentVideoDriver();
    if (!video_driver) {
        return std::monostate{};
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (props == 0) {
        return std::monostate{};
    }

    // X11: retrieve Window ID (XID), Display pointer, and screen number.
    if (SDL_strcmp(video_driver, "x11") == 0) {
        // SDL3 property names for X11 windows.
        // SDL_PROP_WINDOW_X11_WINDOW_NUMBER is the X11 Window (XID).
        // SDL_PROP_WINDOW_X11_DISPLAY_POINTER is the Display*.
        // SDL_PROP_WINDOW_X11_SCREEN_NUMBER is the screen number.
        constexpr const char* kX11WindowProp = "SDL.window.x11.window";
        constexpr const char* kX11DisplayProp = "SDL.window.x11.display";
        constexpr const char* kX11ScreenProp = "SDL.window.x11.screen";

        chess::application::command::X11WindowMetadata metadata;
        metadata.window_id = static_cast<std::uint64_t>(
            SDL_GetNumberProperty(props, kX11WindowProp, 0));
        metadata.display = reinterpret_cast<std::uint64_t>(
            SDL_GetPointerProperty(props, kX11DisplayProp, nullptr));
        metadata.screen_number = static_cast<std::int32_t>(
            SDL_GetNumberProperty(props, kX11ScreenProp, 0));

        if (metadata.window_id != 0) {
            return metadata;
        }
        return std::monostate{};
    }

    // Wayland: retrieve wl_surface pointer.
    if (SDL_strcmp(video_driver, "wayland") == 0) {
        constexpr const char* kWaylandSurfaceProp = "SDL.window.wayland.surface";

        void* surface = SDL_GetPointerProperty(props, kWaylandSurfaceProp, nullptr);
        if (!surface) {
            return std::monostate{};
        }

        chess::application::command::WaylandWindowMetadata metadata;
        metadata.surface = reinterpret_cast<std::uintptr_t>(surface);

        // Retrieve window geometry for the viewport bounds.
        int x = 0, y = 0, w = 0, h = 0;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        metadata.geometry.x = x;
        metadata.geometry.y = y;
        metadata.geometry.width = w;
        metadata.geometry.height = h;

        return metadata;
    }

    return std::monostate{};
}

} // namespace chess::platform::platform_linux
