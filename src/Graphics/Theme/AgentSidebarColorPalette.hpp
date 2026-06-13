#pragma once

#include <imgui.h>

struct AgentSidebarColorPalette {
    ImVec4 info_chat_background{};
    ImVec4 info_chat_border{};
    ImVec4 info_chat_text{};
    
    ImVec4 move_chat_background{};
    ImVec4 move_chat_border{};
    ImVec4 move_chat_text{};
    
    ImVec4 quip_chat_background{};
    ImVec4 quip_chat_border{};
    ImVec4 quip_chat_text{};
    
    ImVec4 reasoning_chat_background{};
    ImVec4 reasoning_chat_border{};
    ImVec4 reasoning_chat_text{};
    
    ImVec4 response_chat_background{};
    ImVec4 response_chat_border{};
    ImVec4 response_chat_text{};
    
    ImVec4 error_chat_background{};
    ImVec4 error_chat_border{};
    ImVec4 error_chat_text{};
    
    ImVec4 warning_icon{};
    ImVec4 checkmark_icon{};
    
    ImVec4 profile_background{};
    ImVec4 profile_border{};
};
