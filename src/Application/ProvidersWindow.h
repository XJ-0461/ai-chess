#pragma once

#include "Application/Provider/ModelProviderConfiguration.hpp"

namespace chess::application {

// Browses the currently configured providers and allows removing them.
void RenderProvidersWindow(bool* show, provider::ProviderConfigurationStore& store);

} // namespace chess::application
