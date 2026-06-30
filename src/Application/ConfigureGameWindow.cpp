#include "ConfigureGameWindow.h"

#include <cstdint>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace chess::application {

namespace {

void RenderAgentSection(const char* label, game::AgentConfiguration& agent) {
    ImGui::SeparatorText(label);
    ImGui::PushID(label);

    ImGui::InputText("Endpoint", &agent.endpoint);

    static const char* const kProviderLabels[] = { "OpenRouter", "AWS Bedrock" };
    int provider_index = static_cast<int>(agent.provider);
    if (ImGui::Combo("Provider", &provider_index, kProviderLabels, IM_ARRAYSIZE(kProviderLabels))) {
        agent.provider = static_cast<game::AgentProvider>(provider_index);
    }

    // Paste a model identifier copied from the Model Browser here.
    ImGui::InputText("Model ID", &agent.model_id);

    ImGui::PopID();
}

} // namespace

void RenderConfigureGameWindow(
    bool* show,
    game::GameConfiguration& config,
    const std::function<void(const game::GameConfiguration&)>& on_create
) {
    if (!*show) {
        return;
    }

    if (ImGui::Begin("Configure New Game", show)) {
        RenderAgentSection("White", config.white);
        RenderAgentSection("Black", config.black);

        ImGui::SeparatorText("Game Rules");
        ImGui::Checkbox("Enable Quips", &config.enable_quip);
        ImGui::Checkbox("Enable Draw Offers", &config.enable_draw_offer);
        ImGui::Checkbox("Enable Resignation", &config.enable_resignation);

        std::int32_t retro = static_cast<std::int32_t>(config.retrospective_turn_count);
        if (ImGui::InputInt("Retrospective Turns", &retro)) {
            if (retro < 0) {
                retro = 0;
            }
            config.retrospective_turn_count = static_cast<std::size_t>(retro);
        }

        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            if (on_create) {
                on_create(config);
            }
        }

        ImGui::End();
    }
}

} // namespace chess::application
