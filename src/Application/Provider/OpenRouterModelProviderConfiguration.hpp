#pragma once

#include <string>

namespace chess::application::provider {

// Credentials for connecting to OpenRouter.
struct OpenRouterModelProviderConfiguration {
    std::string api_key{};
};

} // namespace chess::application::provider
