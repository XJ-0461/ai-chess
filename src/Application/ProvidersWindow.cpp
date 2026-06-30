#include "ProvidersWindow.h"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include <imgui.h>

namespace chess::application {

namespace {

// Show only the tail of a secret so configured providers are recognisable
// without exposing credentials.
[[nodiscard]] std::string MaskSecret(const std::string& secret) {
    if (secret.empty()) {
        return "(not set)";
    }
    if (secret.size() <= 4) {
        return "****";
    }
    return "..." + secret.substr(secret.size() - 4);
}

[[nodiscard]] std::string DetailsOf(const provider::ProviderConfiguration& configuration) {
    return std::visit([](const auto& config) -> std::string {
        using T = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<T, provider::OpenRouterModelProviderConfiguration>) {
            return "key " + MaskSecret(config.api_key);
        } else {
            std::string region = config.region.empty() ? "(no region)" : config.region;
            return region + ", secret " + MaskSecret(config.secret_key);
        }
    }, configuration);
}

} // namespace

void RenderProvidersWindow(bool* show, provider::ProviderConfigurationStore& store) {
    if (!*show) {
        return;
    }

    if (ImGui::Begin("Providers", show)) {
        ImGui::Text("%d configured provider(s)", static_cast<int>(store.providers.size()));

        std::optional<std::size_t> remove_index;

        constexpr ImGuiTableFlags kTableFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##providers", 4, kTableFlags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Kind");
            ImGui::TableSetupColumn("Details");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (std::size_t index = 0; index < store.providers.size(); ++index) {
                const provider::ConfiguredProvider& configured = store.providers[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(configured.name.c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(provider::KindLabel(provider::KindOf(configured.configuration)));

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(DetailsOf(configured.configuration).c_str());

                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Remove")) {
                    remove_index = index;
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (remove_index.has_value()) {
            store.providers.erase(store.providers.begin() + static_cast<std::ptrdiff_t>(*remove_index));
        }
    }
    ImGui::End();
}

} // namespace chess::application
