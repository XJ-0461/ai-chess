#include <imgui.h>

#include <algorithm>
#include <optional>

#include "AgentSidebar.h"
#include "Application/Application.h"
#include "ChatBubble/ReasoningBubble.h"
#include "ChatBubble/ResponseBubble.h"
#include "ChatBubble/QuipBubble.h"
#include "ChatBubble/MoveBubble.h"
#include "ChatBubble/InfoBubble.h"
#include "Graphics/Pieces/PieceAtlas.hpp"
#include "Graphics/Pieces/PieceBorderAtlas.hpp"
#include "Graphics/GLTexture.hpp"
#include "Chess/Move.h"

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
                if (pawn.texture != 0) {
                    const float pawnRenderSize = picSize * 0.8f;
                    const ImVec2 pawnPos{
                        cursor.x + (picSize - pawnRenderSize) * 0.5f,
                        cursor.y + (picSize - pawnRenderSize) * 0.5f
                    };

                    // Atlas is 96x16 (6 pieces * 16px)
                    const ImVec2 uv0{pawn.region.x / 96.0f, pawn.region.y / 16.0f};
                    const ImVec2 uv1{(pawn.region.x + pawn.region.w) / 96.0f, (pawn.region.y + pawn.region.h) / 16.0f};

                    // Pixel-art sprite: force nearest sampling (the ImGui backend
                    // defaults to a linear sampler that would blur it). Restored
                    // to linear when this scope closes.
                    const chess::graphics::NearestSamplerScope nearest{drawList};
                    drawList->AddImage(
                        pawn.texture,
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

            // Right: Future Area — split along the horizontal axis. Top houses
            // the match result (once concluded); bottom houses the opponent
            // pieces this agent has captured, in capture order.
            if (ImGui::BeginChild("FutureArea", ImVec2(0, picSize), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                const float resultHeight = ImGui::GetContentRegionAvail().y * 0.45f;

                // --- Top: match result score ---
                if (ImGui::BeginChild("ResultArea", ImVec2(0, resultHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                    if (m_Trajectory) {
                        if (const auto score = m_Trajectory->GetResultScore(); score && !score->empty()) {
                            if (m_HeaderFont) ImGui::PushFont(m_HeaderFont);
                            const ImVec2 avail = ImGui::GetContentRegionAvail();
                            const ImVec2 textSize = ImGui::CalcTextSize(score->c_str());
                            const ImVec2 origin = ImGui::GetCursorScreenPos();
                            ImGui::GetWindowDrawList()->AddText(
                                ImVec2(origin.x + (avail.x - textSize.x) * 0.5f,
                                       origin.y + (avail.y - textSize.y) * 0.5f),
                                ImGui::GetColorU32(ImGuiCol_Text),
                                score->c_str());
                            if (m_HeaderFont) ImGui::PopFont();
                        }
                    }
                }
                ImGui::EndChild();

                // --- Bottom: captured pieces (opponent-coloured), in capture
                // order. Sized as large as the row allows, overlapping, each
                // backed by a scaled-up white silhouette so it reads against any
                // background and so overlapping neighbours stay distinct.
                if (ImGui::BeginChild("CapturedArea", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
                    if (m_Trajectory && m_CapturedPieceAtlas) {
                        const auto captured = m_Trajectory->GetCapturedPieces();
                        if (!captured.empty()) {
                            // Generic over the piece atlas and the border atlas
                            // (both expose the same GetXxxTexture accessors).
                            const auto texture_from = [](const auto& atlas, Piece p) -> TextureView {
                                if (!atlas) return TextureView{};
                                switch (GetPieceType(p)) {
                                    case Pawn:   return atlas->GetPawnTexture();
                                    case Knight: return atlas->GetKnightTexture();
                                    case Bishop: return atlas->GetBishopTexture();
                                    case Rook:   return atlas->GetRookTexture();
                                    case Queen:  return atlas->GetQueenTexture();
                                    case King:   return atlas->GetKingTexture();
                                    default:     return TextureView{};
                                }
                            };

                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            const chess::graphics::NearestSamplerScope nearest{drawList};

                            const auto draw_sprite = [&](const TextureView& tv, ImVec2 center, float size) {
                                if (tv.texture == 0) return;
                                const ImVec2 uv0{
                                    tv.region.x / PaletteSwappedPieceAtlas::kTotalWidth,
                                    tv.region.y / PaletteSwappedPieceAtlas::kPieceHeight };
                                const ImVec2 uv1{
                                    (tv.region.x + tv.region.w) / PaletteSwappedPieceAtlas::kTotalWidth,
                                    (tv.region.y + tv.region.h) / PaletteSwappedPieceAtlas::kPieceHeight };
                                drawList->AddImage(tv.texture,
                                    ImVec2(center.x - size * 0.5f, center.y - size * 0.5f),
                                    ImVec2(center.x + size * 0.5f, center.y + size * 0.5f),
                                    uv0, uv1);
                            };

                            const int count = static_cast<int>(captured.size());
                            const ImVec2 region = ImGui::GetContentRegionAvail();
                            constexpr float kOverlapAdvance = 0.62f; // ~38% overlap
                            // Largest sprite that fits the row height and the row
                            // width (with overlap). The border atlas is the piece
                            // dilated by 1px, so it's drawn at the SAME rect — the
                            // ring peeks out around the piece automatically.
                            const float byWidth = region.x / (1.0f + (count - 1) * kOverlapAdvance);
                            const float spriteSize = std::max(1.0f, std::min(region.y, byWidth));
                            const float advance = spriteSize * kOverlapAdvance;

                            const ImVec2 start = ImGui::GetCursorScreenPos();
                            const float centerY = start.y + region.y * 0.5f;
                            float centerX = start.x + spriteSize * 0.5f;
                            for (const Piece p : captured) {
                                const ImVec2 center{ centerX, centerY };
                                draw_sprite(texture_from(m_OutlinePieceAtlas, p), center, spriteSize);
                                draw_sprite(texture_from(m_CapturedPieceAtlas, p), center, spriteSize);
                                centerX += advance;
                            }
                        }
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndChild(); // ALWAYS paired, even when BeginChild() returns false
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

            // Transform the pure trajectory into display bubbles, hydrating move
            // bubbles with verified/error state + codified errors from the move
            // log (by id). The backing trajectory is never mutated here.
            m_SidebarTrajectory.Rebuild(*m_Trajectory, m_MoveLog);

            using chess::application::SidebarBubbleKind;
            const AgentSidebarColorPalette& palette = m_ColorPalette;
            ImFont* const headerFont = m_HeaderFont;
            for (const auto& bubble : m_SidebarTrajectory.Bubbles()) {
                switch (bubble.kind) {
                    case SidebarBubbleKind::Info:
                        InfoBubble(bubble.text, false, bubble.move_count)
                            .Render(palette.info_chat_border, palette.info_chat_background, palette.info_chat_text, headerFont);
                        break;
                    case SidebarBubbleKind::Error:
                        InfoBubble(bubble.text, true, bubble.move_count)
                            .Render(palette.error_chat_border, palette.error_chat_background, palette.error_chat_text, headerFont,
                                    chess::graphics::IconId(resources.warning_icon));
                        break;
                    case SidebarBubbleKind::Reasoning:
                        ReasoningBubble(bubble.text, bubble.move_count)
                            .Render(palette.reasoning_chat_border, palette.reasoning_chat_background, palette.reasoning_chat_text, headerFont);
                        break;
                    case SidebarBubbleKind::Response:
                        ResponseBubble(bubble.text, bubble.move_count)
                            .Render(palette.response_chat_border, palette.response_chat_background, palette.response_chat_text, headerFont);
                        break;
                    case SidebarBubbleKind::Quip:
                        QuipBubble(bubble.text, bubble.move_count)
                            .Render(palette.quip_chat_border, palette.quip_chat_background, palette.quip_chat_text, headerFont);
                        break;
                    case SidebarBubbleKind::Move: {
                        ImTextureID icon = 0;
                        if (bubble.move_state == MoveVerificationState::Verified) {
                            icon = chess::graphics::IconId(resources.double_check_icon);
                        } else if (bubble.move_state == MoveVerificationState::Error) {
                            icon = chess::graphics::IconId(resources.warning_icon);
                        }
                        MoveBubble(bubble.text, bubble.move_state, bubble.move_errors, bubble.move_count)
                            .Render(palette.move_chat_border, palette.move_chat_background, palette.move_chat_text, headerFont, icon);
                        break;
                    }
                }
            }

            if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
                ImGui::SetScrollHereY(1.0f);
                m_ScrollToBottom = false;
            }
        }
        ImGui::EndChild(); // ChatHistory: ALWAYS paired, even when BeginChild() returns false
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
