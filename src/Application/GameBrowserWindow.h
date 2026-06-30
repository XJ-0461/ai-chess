#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Game/GameContext.hpp"

namespace chess::application {

// Actions the Game Browser can request of the application.
struct GameBrowserCallbacks {
    std::function<void(const std::string& game_id)> on_play{};
    std::function<void(const std::string& game_id)> on_open_spectator{};
    std::function<void(const std::string& game_id)> on_open_spectator_high_contrast{};
};

void RenderGameBrowserWindow(
    bool* show,
    const std::unordered_map<std::string, std::shared_ptr<chess::game::GameContext>>& games,
    const GameBrowserCallbacks& callbacks
);

} // namespace chess::application
