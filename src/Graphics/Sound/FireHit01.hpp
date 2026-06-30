#pragma once

#include <cstdint>

// Check sound effect, baked into the binary at compile time via #embed.
namespace chess::resources::sound {

static constexpr std::uint8_t kFireHit01WavBytes[] = {
    #embed "Fire_Hit_01.wav"
};

} // namespace chess::resources::sound