#pragma once

#include <cstddef>
#include <string>

namespace chess::game {

// Which agent backend a player uses. Kept in the game domain (rather than
// depending on the application's provider types) so a game configuration is
// self-contained.
enum class AgentProvider {
    OpenRouter,
    AWSBedrock,
};

[[nodiscard]] inline const char* AgentProviderLabel(const AgentProvider provider) {
    switch (provider) {
    case AgentProvider::OpenRouter: return "OpenRouter";
    case AgentProvider::AWSBedrock: return "AWS Bedrock";
    }
    return "Unknown";
}

// Per-player agent setup: where to reach the agent process, which provider
// backend it should use, and which model to run.
struct AgentConfiguration {
    std::string endpoint{};
    AgentProvider provider{AgentProvider::OpenRouter};
    std::string model_id{};
    std::string api_key{}; // resolved from the configured providers at game creation
};

struct GameConfiguration {
    AgentConfiguration white{};
    AgentConfiguration black{};
    bool enable_quip{};
    bool enable_draw_offer{};
    bool enable_resignation{};
    std::size_t retrospective_turn_count{};
};

} // namespace chess::game
