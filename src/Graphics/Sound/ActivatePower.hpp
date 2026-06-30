#pragma once

#include <cstdint>

// Checkmate sound effect, baked into the binary at compile time via #embed.
namespace chess::resources::sound {

static constexpr std::uint8_t kActivatePowerWavBytes[] = {
    #embed "Activate_Power.wav"
};

} // namespace chess::resources::sound