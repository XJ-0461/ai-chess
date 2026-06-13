#pragma once

#include <imgui.h>

#include "Graphics/Theme/AgentSidebarColorPalette.hpp"
#include "Graphics/Board/BoardAtlas.hpp"
#include "Utility/Color.hpp"

namespace chess::style::color {

static constexpr AgentSidebarColorPalette MakeCreamBlueAgentSidebarColorPalette() {
    using namespace chess::utility::color;

    auto to_imvec4 = [](const RGBA& rgba) {
        return ImVec4(rgba.red / 255.0f, rgba.green / 255.0f, rgba.blue / 255.0f, rgba.alpha / 255.0f);
    };

    const ImVec4 board_light = to_imvec4(kCreamBlueBoardColorPalette[0]);
    const ImVec4 board_dark  = to_imvec4(kCreamBlueBoardColorPalette[1]);

    const ImVec4 background_step1 = ColorMixLerp(board_light, board_dark, 0.0f);
    const ImVec4 background_step2 = ColorMixLerp(board_light, board_dark, 1, 3);
    const ImVec4 background_step3 = ColorMixLerp(board_light, board_dark, 2, 3);
    const ImVec4 background_step4 = ColorMixLerp(board_light, board_dark, 1.0f);

    AgentSidebarColorPalette palette{};
    
    palette.info_chat_background = background_step1;
    palette.info_chat_border     = board_dark;
    palette.info_chat_text       = GetAccessibleTextColor(background_step1);

    palette.response_chat_background = background_step1;
    palette.response_chat_border     = board_dark;
    palette.response_chat_text       = GetAccessibleTextColor(background_step1);

    palette.move_chat_background = background_step1;
    palette.move_chat_border     = board_dark;
    palette.move_chat_text       = GetAccessibleTextColor(background_step1);

    palette.error_chat_background = background_step2;
    palette.error_chat_border     = board_dark;
    palette.error_chat_text       = GetAccessibleTextColor(background_step2);

    palette.quip_chat_background = background_step3;
    palette.quip_chat_border     = board_light;
    palette.quip_chat_text       = GetAccessibleTextColor(background_step3);

    palette.reasoning_chat_background = background_step4;
    palette.reasoning_chat_border     = board_light;
    palette.reasoning_chat_text       = GetAccessibleTextColor(background_step4);

    palette.warning_icon   = palette.move_chat_text;
    palette.checkmark_icon = palette.move_chat_text;

    palette.profile_background = background_step1;
    palette.profile_border     = board_dark;

    return palette;
}

static constexpr AgentSidebarColorPalette kCreamBlueAgentSidebarColorPalette = MakeCreamBlueAgentSidebarColorPalette();

} // namespace chess::style::color
