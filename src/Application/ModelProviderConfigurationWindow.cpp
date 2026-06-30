#include "ModelProviderConfigurationWindow.h"

#include <array>
#include <cstring>
#include <string>
#include <utility>

#include <imgui.h>

namespace chess::application {

namespace {

constexpr std::size_t kTextBufferSize = 256;
constexpr std::size_t kRegionBufferSize = 64;

void ClearBuffer(char* buffer) {
    buffer[0] = '\0';
}

} // namespace

void RenderModelProviderConfigurationWindow(bool* show, provider::ProviderConfigurationStore& store) {
    if (!*show) {
        return;
    }

    // Form state persists across frames for the single configuration window.
    static int kind_index = 0; // 0 = OpenRouter, 1 = AWS Bedrock
    static std::array<char, kTextBufferSize> name_buffer{};
    static std::array<char, kTextBufferSize> api_key_buffer{};
    static std::array<char, kTextBufferSize> access_key_buffer{};
    static std::array<char, kTextBufferSize> secret_key_buffer{};
    static std::array<char, kRegionBufferSize> region_buffer{};

    if (ImGui::Begin("Configure Provider", show)) {
        ImGui::InputText("Name", name_buffer.data(), name_buffer.size());

        static const char* const kKindLabels[] = { "OpenRouter", "AWS Bedrock" };
        ImGui::Combo("Provider", &kind_index, kKindLabels, IM_ARRAYSIZE(kKindLabels));

        ImGui::Separator();

        const auto kind = (kind_index == 0)
            ? provider::ProviderKind::OpenRouter
            : provider::ProviderKind::AWSBedrock;

        if (kind == provider::ProviderKind::OpenRouter) {
            ImGui::InputText("API Key", api_key_buffer.data(), api_key_buffer.size(), ImGuiInputTextFlags_Password);
        } else {
            ImGui::InputText("Access Key", access_key_buffer.data(), access_key_buffer.size());
            ImGui::InputText("Secret Key", secret_key_buffer.data(), secret_key_buffer.size(), ImGuiInputTextFlags_Password);
            ImGui::InputText("Region", region_buffer.data(), region_buffer.size());
        }

        ImGui::Separator();

        const bool has_name = name_buffer[0] != '\0';
        ImGui::BeginDisabled(!has_name);
        if (ImGui::Button("Save")) {
            provider::ConfiguredProvider configured;
            configured.name = name_buffer.data();

            if (kind == provider::ProviderKind::OpenRouter) {
                provider::OpenRouterModelProviderConfiguration configuration;
                configuration.api_key = api_key_buffer.data();
                configured.configuration = std::move(configuration);
            } else {
                provider::AWSBedrockModelProviderConfiguration configuration;
                configuration.access_key = access_key_buffer.data();
                configuration.secret_key = secret_key_buffer.data();
                configuration.region = region_buffer.data();
                configured.configuration = std::move(configuration);
            }

            store.providers.push_back(std::move(configured));

            ClearBuffer(name_buffer.data());
            ClearBuffer(api_key_buffer.data());
            ClearBuffer(access_key_buffer.data());
            ClearBuffer(secret_key_buffer.data());
            ClearBuffer(region_buffer.data());
        }
        ImGui::EndDisabled();

        if (!has_name) {
            ImGui::SameLine();
            ImGui::TextDisabled("(name required)");
        }
    }
    ImGui::End();
}

} // namespace chess::application
