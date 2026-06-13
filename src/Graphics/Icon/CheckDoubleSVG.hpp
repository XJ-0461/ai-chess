#pragma once

#include <cstdint>
#include <span>

static constexpr std::uint8_t kCheckDoubleSvgBytes[] = {
    #embed "CheckDouble.svg"
};

static constexpr std::span<const std::uint8_t> kCheckDoubleSvg { kCheckDoubleSvgBytes, sizeof(kCheckDoubleSvgBytes) };
