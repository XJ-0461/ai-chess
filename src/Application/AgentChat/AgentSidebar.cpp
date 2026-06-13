#include <imgui.h>

#include "AgentSidebar.h"
#include "Application/Application.h"
#include "ChatBubble/ReasoningBubble.h"
#include "ChatBubble/ResponseBubble.h"
#include "ChatBubble/QuipBubble.h"
#include "ChatBubble/MoveBubble.h"
#include "ChatBubble/InfoBubble.h"
#include "Graphics/Pieces/PieceAtlas.hpp"

AgentSidebar::AgentSidebar(const std::string& title, const std::string& colorName)
    : m_Title(title), m_ColorName(colorName) {
}

void AgentSidebar::ClearChat() {
    if (m_Trajectory) {
        std::lock_guard<std::mutex> lock(m_Trajectory->mtx);
        m_Trajectory->events.clear();
        m_Trajectory->state = AgentState::Idle;
    }
}

void AgentSidebar::Render() {
    // Render as a child window to integrate into the Layout's table cells
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    
    // Use the title for the child ID to keep state separate if multiple sidebars exist
    if (ImGui::BeginChild(m_Title.c_str(), ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {

        if (!m_Trajectory) {
            ImGui::Text("Agent not initialized.");
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        // --- Header Redesign ---
        ImGui::Spacing();
        ImGui::Indent(8.0f);

        const float availWidth = ImGui::GetContentRegionAvail().x - 8.0f; // Account for indent
        const float picSize = std::clamp(availWidth * 0.25f, 48.0f, 80.0f);
        constexpr float padding = 8.0f;

        // Determine border color from palette
        const ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(m_ColorPalette.profile_border);
        const ImU32 profile_bg_color = ImGui::ColorConvertFloat4ToU32(m_ColorPalette.profile_background);

        ImGui::BeginGroup();
        {
            // Left: Profile Pic Placeholder / Pawn Texture
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(picSize, picSize));
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(
                cursor,
                ImVec2{cursor.x + picSize, cursor.y + picSize},
                profile_bg_color,
                0.0f
            );

            drawList->AddRect(
                cursor, 
                ImVec2(cursor.x + picSize, cursor.y + picSize), 
                borderColor, 
                0.0f,           // No rounding
                ImDrawFlags_None,
                3.5f            // Thicker border
            );

            if (m_PieceAtlas) {
                const TextureView pawn = m_PieceAtlas->GetPawnTexture();
                if (pawn.texture) {
                    const float pawnRenderSize = picSize * 0.8f;
                    const ImVec2 pawnPos{
                        cursor.x + (picSize - pawnRenderSize) * 0.5f,
                        cursor.y + (picSize - pawnRenderSize) * 0.5f
                    };

                    // Atlas is 96x16 (6 pieces * 16px)
                    const ImVec2 uv0{pawn.region.x / 96.0f, pawn.region.y / 16.0f};
                    const ImVec2 uv1{(pawn.region.x + pawn.region.w) / 96.0f, (pawn.region.y + pawn.region.h) / 16.0f};

                    drawList->AddImage(
                        static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pawn.texture)),
                        pawnPos,
                        ImVec2(pawnPos.x + pawnRenderSize, pawnPos.y + pawnRenderSize),
                        uv0,
                        uv1
                    );
                }
            } else {
                // Fallback label
                constexpr auto label = "PFP";
                const ImVec2 labelSize = ImGui::CalcTextSize(label);
                drawList->AddText(
                    ImVec2(cursor.x + (picSize - labelSize.x) * 0.5f, cursor.y + (picSize - labelSize.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_Text, 0.3f),
                    label
                );
            }

            ImGui::SameLine(0, padding);

            // Right: Future Area
            if (ImGui::BeginChild("FutureArea", ImVec2(0, picSize), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                // ImGui::TextDisabled("Future...");
                ImGui::EndChild();
            }
        }
        ImGui::EndGroup();

        ImGui::Spacing();

        // Model Name with Custom Font
        if (m_HeaderFont) ImGui::PushFont(m_HeaderFont);
        ImGui::Text("%s", m_Trajectory->modelName.c_str());
        if (m_HeaderFont) ImGui::PopFont();

        ImGui::Unindent(8.0f);
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Spacing();

        // --- Chat Logs Child Area ---
        if (ImGui::BeginChild((m_Title + "##ChatHistory").c_str(), ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            const auto& resources = Application::Get().GetTextureResources();
            {
                std::lock_guard<std::mutex> lock(m_Trajectory->mtx);
                for (const auto& ev : m_Trajectory->events) {
                    if (ev.type == "reasoning") {
                        ReasoningBubble(ev.message).Render(m_ColorPalette.reasoning_chat_border, m_ColorPalette.reasoning_chat_background, m_ColorPalette.reasoning_chat_text, m_HeaderFont);
                    } else if (ev.type == "response") {
                        ResponseBubble(ev.message).Render(m_ColorPalette.response_chat_border, m_ColorPalette.response_chat_background, m_ColorPalette.response_chat_text, m_HeaderFont);
                    } else if (ev.type == "quip") {
                        QuipBubble(ev.message).Render(m_ColorPalette.quip_chat_border, m_ColorPalette.quip_chat_background, m_ColorPalette.quip_chat_text, m_HeaderFont);
                    } else if (ev.type == "move") {
                        std::shared_ptr<SDL_Texture> icon = nullptr;
                        if (ev.verificationState == MoveVerificationState::Verified) icon = resources.double_check_icon;
                        if (ev.verificationState == MoveVerificationState::Error)    icon = resources.warning_icon;
                        MoveBubble(ev.message, ev.verificationState, ev.errorMessage).Render(m_ColorPalette.move_chat_border, m_ColorPalette.move_chat_background, m_ColorPalette.move_chat_text, m_HeaderFont, icon);
                    } else if (ev.type == "error") {
                        InfoBubble(ev.message, true).Render(m_ColorPalette.error_chat_border, m_ColorPalette.error_chat_background, m_ColorPalette.error_chat_text, m_HeaderFont, resources.warning_icon);
                    } else {
                        InfoBubble(ev.message).Render(m_ColorPalette.info_chat_border, m_ColorPalette.info_chat_background, m_ColorPalette.info_chat_text, m_HeaderFont);
                    }
                }
            }

            if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
                ImGui::SetScrollHereY(1.0f);
                m_ScrollToBottom = false;
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
