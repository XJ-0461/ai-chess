#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Game/Configuration/GameConfiguration.hpp"
#include "Game/Execution/GameOrchestrator.hpp"

namespace chess::application {
    void RenderGameBrowserWindow(bool* show, 
        const std::unordered_map<std::string, game::GameConfiguration>& configs,
        const std::unordered_map<std::string, std::shared_ptr<game::execution::GameOrchestrator>>& games);
}
