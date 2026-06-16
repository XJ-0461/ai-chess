#include "GameBrowserWindow.h"
#include <imgui.h>

namespace chess::application {

void RenderGameBrowserWindow(bool* show, 
    const std::unordered_map<std::string, game::GameConfiguration>& configs,
    const std::unordered_map<std::string, std::shared_ptr<game::execution::GameOrchestrator>>& games) 
{
    if (!*show) return;

    if (ImGui::Begin("Game Browser", show)) {
        if (configs.empty() && games.empty()) {
            ImGui::Text("No games available.");
        }

        // Combine for browsing - simplified for now
        for (const auto& [id, config] : configs) {
            ImGui::PushID(id.c_str());
            ImGui::BeginChild(id.c_str(), ImVec2(0, 100), true);

            ImGui::Text("Game ID: %s", id.c_str());
            ImGui::Separator();
            ImGui::Columns(2, "cols", false);
            ImGui::Text("White: %s", config.white_endpoint.c_str());
            ImGui::NextColumn();
            ImGui::Text("Black: %s", config.black_endpoint.c_str());
            ImGui::Columns(1);

            if (games.contains(id)) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Status: Configured");
            }

            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Spacing();
        }

        for (const auto& [id, game] : games) {
            if (configs.contains(id)) continue; // already shown

            ImGui::PushID(id.c_str());
            ImGui::BeginChild(id.c_str(), ImVec2(0, 80), true);
            ImGui::Text("Game ID: %s", id.c_str());
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Running (Actor)");
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Spacing();
        }

        ImGui::End();
    }
}

} // namespace chess::application
