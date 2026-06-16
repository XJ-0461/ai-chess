#include "ConfigureGameWindow.h"

#include <cstdint>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace chess::application {

void RenderConfigureGameWindow(bool* show, game::GameConfiguration& config) {
    if (!*show) {
        return;
    }

    if (ImGui::Begin("Configure New Game", show)) {
        ImGui::Text("Agent Endpoints");
        ImGui::InputText("White Endpoint", &config.white_endpoint);
        ImGui::InputText("Black Endpoint", &config.black_endpoint);

        ImGui::Separator();
        ImGui::Text("Game Rules");
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
        ImGui::BeginDisabled(); // Not hooked up yet
        if (ImGui::Button("Play", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            // Future: Spawn actor
        }
        ImGui::EndDisabled();

        ImGui::End();
    }
}

} // namespace chess::application
