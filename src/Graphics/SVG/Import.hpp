#pragma once

#include <memory>
#include <span>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "Utility/SDL3_STD.hpp"

namespace chess::graphics::svg {

inline std::shared_ptr<SDL_Texture> LoadSVGTextureFromMemory(
    const std::shared_ptr<SDL_Renderer>& renderer,
    const std::span<const std::uint8_t>& svg
) {

    // 1. Create an SDL_IOStream from the memory buffer
    SDL_IOStream* io = SDL_IOFromConstMem(svg.data(), svg.size_bytes());
    if (!io) {
        SDL_Log("Failed to create IOStream: %s", SDL_GetError());
        return nullptr;
    }

    // 2. Load the SVG into a surface, auto-sizing or explicitly scaling
    // Note: Pass 0 for width/height to use the SVG's default dimensions
    SDL_Surface* surface = IMG_LoadSVG_IO(io);

    // The IOStream is no longer needed once the surface is created
    SDL_CloseIO(io);

    if (!surface) {
        SDL_Log("Failed to load SVG from memory: %s", SDL_GetError());
        return nullptr;
    }

    const std::shared_ptr<SDL_Texture> texture{
        SDL_CreateTextureFromSurface(renderer.get(), surface),
        SDLTextureDeleter
    };

    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_Log("Failed to create texture from surface: %s", SDL_GetError());
    }

    return texture;
}

} // namespace chess::graphics::svg
