#pragma once

#include <cstdint>

// Error sound effect (invalid move, agent failure), baked into the binary at compile time via #embed.
namespace chess::resources::sound {

static constexpr std::uint8_t kNoEntryWavBytes[] = {
    #embed "No_Entry.wav"
};

} // namespace chess::resources::sound