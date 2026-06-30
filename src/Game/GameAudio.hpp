#pragma once

#include <string>

#include <SDL3_mixer/SDL_mixer.h>

namespace chess::game {

// Per-game audio buses. Each game owns an independent SFX mixer and Music mixer
// so their volumes can be controlled separately from every other game's. (These
// currently render to the shared default output device; OS-level independent
// capture would additionally require binding each mixer to its own virtual sink
// — deliberately out of scope for this first pass.)
//
// The MIX_Mixer objects are created on the main thread (see
// Application::ProcessPendingAudioInit) because MIX_CreateMixerDevice must be;
// the gains can be set from any thread once the mixers exist.
struct GameAudio {
    MIX_Mixer* sfx_mixer{nullptr};
    MIX_Mixer* music_mixer{nullptr};

    // One-shot effects loaded into sfx_mixer.
    MIX_Audio* capture_sfx{nullptr};        // regular piece capture
    MIX_Audio* queen_capture_sfx{nullptr};  // queen captured
    MIX_Audio* check_sfx{nullptr};          // check delivered
    MIX_Audio* checkmate_sfx{nullptr};      // checkmate delivered
    MIX_Audio* error_sfx{nullptr};          // invalid move / agent error

    // Last-set bus gains in [0, 1]; also the backing state for the UI sliders.
    float sfx_gain{1.0f};
    float music_gain{1.0f};

    // Human-readable stream identifiers, e.g. "Chess(1234).Game(match).SoundEffects".
    std::string sfx_name{};
    std::string music_name{};

    GameAudio() = default;
    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;

    // Destroying a mixer must happen on the main thread (same as creation); the
    // owning GameContext is released on the main thread at shutdown.
    ~GameAudio() {
        // Destroy the mixers first (this stops/frees any tracks still playing a
        // MIX_Audio), then free the audio buffers they referenced.
        if (sfx_mixer) {
            MIX_DestroyMixer(sfx_mixer);
        }
        if (music_mixer) {
            MIX_DestroyMixer(music_mixer);
        }
        if (capture_sfx) {
            MIX_DestroyAudio(capture_sfx);
        }
        if (queen_capture_sfx) {
            MIX_DestroyAudio(queen_capture_sfx);
        }
        if (check_sfx) {
            MIX_DestroyAudio(check_sfx);
        }
        if (checkmate_sfx) {
            MIX_DestroyAudio(checkmate_sfx);
        }
        if (error_sfx) {
            MIX_DestroyAudio(error_sfx);
        }
    }

    void SetSfxGain(float gain) {
        sfx_gain = gain;
        if (sfx_mixer) {
            MIX_SetMixerGain(sfx_mixer, gain);
        }
    }

    void SetMusicGain(float gain) {
        music_gain = gain;
        if (music_mixer) {
            MIX_SetMixerGain(music_mixer, gain);
        }
    }
};

} // namespace chess::game
