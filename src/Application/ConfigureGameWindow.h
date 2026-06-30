#pragma once

#include <functional>

#include "Game/Configuration/GameConfiguration.hpp"

namespace chess::application {
    void RenderConfigureGameWindow(
        bool* show,
        game::GameConfiguration& config,
        const std::function<void(const game::GameConfiguration&)>& on_create
    );
}
