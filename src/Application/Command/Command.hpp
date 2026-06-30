#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "Game/Configuration/GameConfiguration.hpp"
#include "Application/Provider/ModelProviderConfiguration.hpp"

namespace chess::application::command {

// A small, ordered command surface that can be fed at startup (from a JSON
// file) and, later, programmatically from other sources. Each command is run to
// completion before the next begins.

struct ConfigureProviderCommand {
    provider::ConfiguredProvider provider{};
};

struct ConfigureGameCommand {
    std::string game_id{};
    chess::game::GameConfiguration configuration{};
};

struct StartGameCommand {
    std::string game_id{};
};

// Query the (eventual) result of a match. Handled asynchronously: the executor
// waits for the game to conclude and then returns the MatchResult to the caller.
struct QueryMatchResultCommand {
    std::string game_id{};
};

// Window configuration for spawned views (spectator windows, etc.).
struct WindowSize {
    std::uint32_t width{1920};
    std::uint32_t height{1080};
};

struct WindowPosition {
    std::int32_t x{0};
    std::int32_t y{0};
};

struct WindowConfiguration {
    std::string type{"floating"};  // "floating", "docked", etc.
    WindowSize size{};
    WindowPosition position{};
    std::vector<std::string> imgui_flags{};  // e.g., "ImGuiWindowFlags_NoTitleBar"
};

// Opens a spectator view for a game. Returns window metadata (ImGui window ID
// and OS-level window identifiers) for programmatic screen capture.
struct OpenSpectatorViewCommand {
    std::string game_id{};
    std::string game_view_theme{"default"};  // "default", "high_contrast"
    WindowConfiguration window_configuration{};
};

// Toggles the move history bar on a spectator view by its ImGui window ID.
struct SetMoveHistoryBarCommand {
    std::string window_id{};
    bool enable_move_history_bar{true};
};

using Command = std::variant<
    ConfigureProviderCommand,
    ConfigureGameCommand,
    StartGameCommand,
    QueryMatchResultCommand,
    OpenSpectatorViewCommand,
    SetMoveHistoryBarCommand
>;

using CommandList = std::vector<Command>;

} // namespace chess::application::command
