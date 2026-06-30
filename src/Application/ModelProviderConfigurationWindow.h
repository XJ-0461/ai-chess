#pragma once

#include "Application/Provider/ModelProviderConfiguration.hpp"

namespace chess::application {

// Form to configure a new model provider (OpenRouter or AWS Bedrock). On save
// the configured provider is appended to the store.
void RenderModelProviderConfigurationWindow(bool* show, provider::ProviderConfigurationStore& store);

} // namespace chess::application
