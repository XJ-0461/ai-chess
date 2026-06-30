#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <variant>

#include <nlohmann/json.hpp>

namespace chess::application::command {

// Concept for command response types - all must have a numeric status code.
template <typename T>
concept CommandResponseType = requires(T response) {
    { response.code } -> std::convertible_to<std::uint32_t>;
};

// X11 window metadata for programmatic screen capture.
struct X11WindowMetadata {
    std::uint64_t window_id{0};     // X11 Window (XID)
    std::uint64_t display{0};       // Display* as uintptr_t
    std::int32_t screen_number{0};
};

inline void to_json(nlohmann::json& j, const X11WindowMetadata& m) {
    j = nlohmann::json{
        {"type", "x11_window"},
        {"detail", {
            {"window_id", m.window_id},
            {"display", m.display},
            {"screen_number", m.screen_number}
        }}
    };
}

// Wayland window metadata for programmatic screen capture.
struct WaylandWindowMetadata {
    std::uintptr_t surface{0};      // wl_surface* as uintptr_t
    struct Geometry {
        std::int32_t x{0};
        std::int32_t y{0};
        std::int32_t width{0};
        std::int32_t height{0};
    } geometry{};
};

inline void to_json(nlohmann::json& j, const WaylandWindowMetadata& m) {
    j = nlohmann::json{
        {"type", "wayland_surface"},
        {"detail", {
            {"surface", m.surface},
            {"geometry", {
                {"x", m.geometry.x},
                {"y", m.geometry.y},
                {"width", m.geometry.width},
                {"height", m.geometry.height}
            }}
        }}
    };
}

// OS-level window metadata. monostate indicates no metadata available (e.g.,
// unsupported platform or failure to retrieve).
using OSWindowMetadata = std::variant<std::monostate, X11WindowMetadata, WaylandWindowMetadata>;

inline void to_json(nlohmann::json& j, const OSWindowMetadata& m) {
    std::visit([&j](const auto& metadata) {
        using T = std::decay_t<decltype(metadata)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            j = nlohmann::json{{"type", "unavailable"}};
        } else {
            to_json(j, metadata);
        }
    }, m);
}

// ImGui window metadata (the logical ImGui window name/ID).
struct ImGuiWindowMetadata {
    std::string window_id{};  // The ImGui window title/ID string
};

inline void to_json(nlohmann::json& j, const ImGuiWindowMetadata& m) {
    j = nlohmann::json{{"window_id", m.window_id}};
}

// Combined window metadata: both the ImGui-level identifier and the OS-level
// native window/surface information.
struct WindowMetadata {
    ImGuiWindowMetadata imgui_metadata{};
    OSWindowMetadata os_metadata{};
};

inline void to_json(nlohmann::json& j, const WindowMetadata& m) {
    j = nlohmann::json{
        {"imgui_metadata", m.imgui_metadata},
        {"os_metadata", m.os_metadata}
    };
}

// Response returned when a spectator view is successfully opened.
struct OpenSpectatorViewResponse {
    std::uint32_t code{200};
    WindowMetadata window_metadata{};
};

inline void to_json(nlohmann::json& j, const OpenSpectatorViewResponse& r) {
    j = nlohmann::json{
        {"code", r.code},
        {"window_metadata", r.window_metadata}
    };
}

// Static assertion to verify the concept works.
static_assert(CommandResponseType<OpenSpectatorViewResponse>);

// Simple response containing only a status code (e.g., for set_move_history_bar).
struct SimpleCodeResponse {
    std::uint32_t code{200};
};

inline void to_json(nlohmann::json& j, const SimpleCodeResponse& r) {
    j = nlohmann::json{{"code", r.code}};
}

static_assert(CommandResponseType<SimpleCodeResponse>);

} // namespace chess::application::command
