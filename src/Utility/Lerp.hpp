#pragma once

#include <imgui.h>

namespace chess::util {

constexpr ImVec4 ConstexprImLerp(const ImVec4& a, const ImVec4& b, const float t) {
    return ImVec4{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

} // namespace chess::util
