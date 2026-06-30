#pragma once

#include <string>
#include <variant>
#include <vector>

#include "Application/Provider/OpenRouterModelProviderConfiguration.hpp"
#include "Application/Provider/AWSBedrockModelProviderConfiguration.hpp"

namespace chess::application::provider {

enum class ProviderKind {
    OpenRouter,
    AWSBedrock,
};

[[nodiscard]] inline const char* KindLabel(const ProviderKind kind) {
    switch (kind) {
    case ProviderKind::OpenRouter: return "OpenRouter";
    case ProviderKind::AWSBedrock: return "AWS Bedrock";
    }
    return "Unknown";
}

using ProviderConfiguration = std::variant<
    OpenRouterModelProviderConfiguration,
    AWSBedrockModelProviderConfiguration
>;

[[nodiscard]] inline ProviderKind KindOf(const ProviderConfiguration& configuration) {
    return std::holds_alternative<OpenRouterModelProviderConfiguration>(configuration)
        ? ProviderKind::OpenRouter
        : ProviderKind::AWSBedrock;
}

// A user-named, configured provider that can later be referenced when setting
// up an agent so it knows how to connect.
struct ConfiguredProvider {
    std::string name{};
    ProviderConfiguration configuration{};
};

// Application-level store of configured providers. UI-thread owned for now.
struct ProviderConfigurationStore {
    std::vector<ConfiguredProvider> providers{};
};

} // namespace chess::application::provider
