#pragma once

#include <string>

namespace chess::application::provider {

// Credentials for connecting to AWS Bedrock.
struct AWSBedrockModelProviderConfiguration {
    std::string access_key{};
    std::string secret_key{};
    std::string region{}; // e.g. "us-east-1"; required by the Bedrock client
};

} // namespace chess::application::provider
