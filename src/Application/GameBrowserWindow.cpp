#include "GameBrowserWindow.h"

#include <imgui.h>

#include "Game/GameAudio.hpp"

namespace chess::application {

void RenderGameBrowserWindow(
    bool* show,
    const std::unordered_map<std::string, std::shared_ptr<chess::game::GameContext>>& games,
    const GameBrowserCallbacks& callbacks
) {
    if (!*show) {
        return;
    }

    if (ImGui::Begin("Game Browser", show)) {
        if (games.empty()) {
            ImGui::Text("No games. Create one via File > New.");
        }

        for (const auto& [id, context] : games) {
            ImGui::PushID(id.c_str());
            ImGui::BeginChild("##game_card", ImVec2(0, 220), true);

            ImGui::Text("Game: %s", id.c_str());
            ImGui::Separator();

            const chess::game::GameConfiguration& config = context->configuration;
            ImGui::Text("White: %s [%s] %s",
                config.white.endpoint.empty() ? "(no endpoint)" : config.white.endpoint.c_str(),
                chess::game::AgentProviderLabel(config.white.provider),
                config.white.model_id.empty() ? "(no model)" : config.white.model_id.c_str());
            ImGui::Text("Black: %s [%s] %s",
                config.black.endpoint.empty() ? "(no endpoint)" : config.black.endpoint.c_str(),
                chess::game::AgentProviderLabel(config.black.provider),
                config.black.model_id.empty() ? "(no model)" : config.black.model_id.c_str());
            ImGui::Text("Quips: %s   Draw offers: %s   Resignation: %s   Retrospective turns: %zu",
                config.enable_quip ? "on" : "off",
                config.enable_draw_offer ? "on" : "off",
                config.enable_resignation ? "on" : "off",
                config.retrospective_turn_count);

            const chess::game::GameLifecyclePhase phase = context->phase
                ? context->phase->load()
                : chess::game::GameLifecyclePhase::Initial;
            ImGui::Text("Status: %s", chess::game::GameLifecyclePhaseLabel(phase));

            // Per-game audio volume buses (independent of every other game).
            if (context->audio) {
                chess::game::GameAudio& audio = *context->audio;
                ImGui::Separator();
                ImGui::SetNextItemWidth(160.0f);
                if (float sfx = audio.sfx_gain; ImGui::SliderFloat("SFX volume", &sfx, 0.0f, 1.0f, "%.2f")) {
                    audio.SetSfxGain(sfx);
                }
                ImGui::SetNextItemWidth(160.0f);
                if (float music = audio.music_gain; ImGui::SliderFloat("Music volume", &music, 0.0f, 1.0f, "%.2f")) {
                    audio.SetMusicGain(music);
                }
            }

            ImGui::Spacing();

            if (ImGui::Button("Play")) {
                if (callbacks.on_play) {
                    callbacks.on_play(id);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Spectator View")) {
                if (callbacks.on_open_spectator) {
                    callbacks.on_open_spectator(id);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Spectator View (High Contrast)")) {
                if (callbacks.on_open_spectator_high_contrast) {
                    callbacks.on_open_spectator_high_contrast(id);
                }
            }

            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
    ImGui::End();
}

} // namespace chess::application
