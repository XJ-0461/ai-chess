#pragma once

#include <cstdint>

// The capture/confirm sound effect, baked into the binary at compile time via
// #embed (the build adds resources/sounds/ to the embed search path, mirroring
// how Board.rgba / fonts are embedded). Wrapped in an SDL_IOStream and handed to
// MIX_LoadAudio_IO at runtime.
namespace chess::resources::sound {

static constexpr std::uint8_t kConfirm01WavBytes[] = {
    #embed "Confirm_01.wav"
};

} // namespace chess::resources::sound
