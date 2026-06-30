#pragma once

#include <cstdint>

// Queen capture sound effect, baked into the binary at compile time via #embed.
namespace chess::resources::sound {

static constexpr std::uint8_t kCollectPoint2WavBytes[] = {
    #embed "Collect_Point_2.wav"
};

} // namespace chess::resources::sound