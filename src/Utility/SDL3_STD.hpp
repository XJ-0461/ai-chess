#pragma once

#include <SDL3/SDL.h>

constexpr auto SDLTextureDeleter =
    [](SDL_Texture* texture) -> void {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    };

constexpr auto SDLSurfaceDeleter =
    [](SDL_Surface* surface) -> void {
        if (surface) {
            SDL_DestroySurface(surface);
        }
    };
