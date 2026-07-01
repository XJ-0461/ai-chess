#include "Application/Command/CommandFile.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace chess::application::command {

namespace {

[[nodiscard]] chess::game::AgentProvider ParseAgentProvider(const std::string& kind) {
    return kind == "AWSBedrock"
        ? chess::game::AgentProvider::AWSBedrock
        : chess::game::AgentProvider::OpenRouter;
}

[[nodiscard]] provider::ProviderKind ParseProviderKind(const std::string& kind) {
    return kind == "AWSBedrock"
        ? provider::ProviderKind::AWSBedrock
        : provider::ProviderKind::OpenRouter;
}

[[nodiscard]] chess::game::AgentConfiguration ParseAgent(const nlohmann::json& json) {
    chess::game::AgentConfiguration agent;
    agent.endpoint = json.value("endpoint", std::string{});
    agent.provider = ParseAgentProvider(json.value("provider", std::string{"OpenRouter"}));
    agent.model_id = json.value("model_id", std::string{});
    return agent;
}

[[nodiscard]] ConfigureProviderCommand ParseConfigureProvider(const nlohmann::json& detail) {
    ConfigureProviderCommand command;
    command.provider.name = detail.value("name", std::string{});

    const provider::ProviderKind kind = ParseProviderKind(detail.value("kind", std::string{"OpenRouter"}));
    if (kind == provider::ProviderKind::OpenRouter) {
        provider::OpenRouterModelProviderConfiguration configuration;
        configuration.api_key = detail.value("api_key", std::string{});
        command.provider.configuration = std::move(configuration);
    } else {
        provider::AWSBedrockModelProviderConfiguration configuration;
        configuration.access_key = detail.value("access_key", std::string{});
        configuration.secret_key = detail.value("secret_key", std::string{});
        configuration.region = detail.value("region", std::string{});
        command.provider.configuration = std::move(configuration);
    }
    return command;
}

[[nodiscard]]
ConfigureGameCommand ParseConfigureGame(const nlohmann::json& detail) {
    ConfigureGameCommand command;
    command.game_id = detail.value("game_id", std::string{});
    if (detail.contains("white")) {
        command.configuration.white = ParseAgent(detail.at("white"));
    }
    if (detail.contains("black")) {
        command.configuration.black = ParseAgent(detail.at("black"));
    }
    command.configuration.enable_quip = detail.value("enable_quip", false);
    command.configuration.enable_draw_offer = detail.value("enable_draw_offer", false);
    command.configuration.enable_resignation = detail.value("enable_resignation", false);
    command.configuration.retrospective_turn_count =
        detail.value("retrospective_turn_count", static_cast<std::size_t>(0));
    return command;
}

[[nodiscard]]
OpenSpectatorViewCommand ParseOpenSpectatorView(const nlohmann::json& detail) {
    OpenSpectatorViewCommand command;
    command.game_id = detail.value("game_id", std::string{});
    command.game_view_theme = detail.value("game_view_theme", std::string{"default"});

    if (detail.contains("window_configuration")) {
        const auto& wc = detail.at("window_configuration");
        command.window_configuration.type = wc.value("type", std::string{"floating"});

        if (wc.contains("window_size")) {
            const auto& ws = wc.at("window_size");
            command.window_configuration.size.width = ws.value("width", 1920u);
            command.window_configuration.size.height = ws.value("height", 1080u);
        }
        if (wc.contains("window_position")) {
            const auto& wp = wc.at("window_position");
            command.window_configuration.position.x = wp.value("x", 0);
            command.window_configuration.position.y = wp.value("y", 0);
        }
        if (wc.contains("imgui_flags") && wc.at("imgui_flags").is_array()) {
            for (const auto& flag : wc.at("imgui_flags")) {
                if (flag.is_string()) {
                    command.window_configuration.imgui_flags.push_back(flag.get<std::string>());
                }
            }
        }
    }
    return command;
}

} // namespace

std::optional<CommandList> LoadCommandsFromFile(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open command file: " + path;
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(buffer.str());
    } catch (const std::exception& e) {
        error = std::string("Failed to parse command file JSON: ") + e.what();
        return std::nullopt;
    }

    if (!root.contains("commands") || !root.at("commands").is_array()) {
        error = "Command file must contain a 'commands' array.";
        return std::nullopt;
    }

    CommandList commands{};
    for (const auto& entry : root.at("commands")) {
        const std::string type = entry.value("type", std::string{});
        const nlohmann::json detail = entry.value("detail", nlohmann::json::object());

        if (type == "configure_provider") {
            commands.emplace_back(ParseConfigureProvider(detail));
        } else if (type == "configure_game") {
            commands.emplace_back(ParseConfigureGame(detail));
        } else if (type == "start_game") {
            commands.emplace_back(StartGameCommand{ detail.value("game_id", std::string{}) });
        } else if (type == "query_match_result") {
            commands.emplace_back(QueryMatchResultCommand{ detail.value("game_id", std::string{}) });
        } else if (type == "open_spectator_view") {
            commands.emplace_back(ParseOpenSpectatorView(detail));
        } else if (type == "spectator_view::set_move_history_bar") {
            commands.emplace_back(SetMoveHistoryBarCommand{
                detail.value("window_id", std::string{}),
                detail.value("enable_move_history_bar", true)
            });
        } else {
            error = "Unknown command type: '" + type + "'";
            return std::nullopt;
        }
    }

    return commands;
}

} // namespace chess::application::command
