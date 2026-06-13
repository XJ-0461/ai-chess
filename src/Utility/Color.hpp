#pragma once

#include <imgui.h>

namespace chess::utility::color {

/**
 * @brief Linearly interpolates between two ImVec4 colors.
 */
constexpr ImVec4 ColorMixLerp(const ImVec4& from_color, const ImVec4& to_color, const float interpolation_factor) {
    return ImVec4{
        from_color.x + (to_color.x - from_color.x) * interpolation_factor,
        from_color.y + (to_color.y - from_color.y) * interpolation_factor,
        from_color.z + (to_color.z - from_color.z) * interpolation_factor,
        1.0f
    };
}

/**
 * @brief Helper for step-based linear interpolation.
 */
constexpr ImVec4 ColorMixLerp(const ImVec4& from_color, const ImVec4& to_color, const std::size_t current_step, const std::size_t total_steps) {
    const float interpolation_factor = total_steps > 1 
        ? static_cast<float>(current_step) / static_cast<float>(total_steps - 1) 
        : 0.0f;
    return ColorMixLerp(from_color, to_color, interpolation_factor);
}

/**
 * @brief Calculates a high-contrast text color (black or white) based on background luminance.
 */
constexpr ImVec4 GetAccessibleTextColor(const ImVec4& background_color) {
    // Relative luminance approximation for constexpr usage
    auto apply_gamma_approximation = [](const float color_channel) {
        return color_channel * color_channel;
    };

    const float luminance_red   = apply_gamma_approximation(background_color.x);
    const float luminance_green = apply_gamma_approximation(background_color.y);
    const float luminance_blue  = apply_gamma_approximation(background_color.z);

    const float relative_luminance = 0.2126f * luminance_red + 0.7152f * luminance_green + 0.0722f * luminance_blue;

    const bool is_background_bright = relative_luminance > 0.179f;
    return is_background_bright ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

} // namespace chess::utility::color
