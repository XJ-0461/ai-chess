#pragma once

#include <cstdint>
#include <span>

static constexpr std::uint8_t kWarningSvgBytes[] = {
    #embed "Warning.svg"
};

static constexpr std::span<const std::uint8_t> kWarningSvg { kWarningSvgBytes, sizeof(kWarningSvgBytes) };
