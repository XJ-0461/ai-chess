#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "Graphics/GLTexture.hpp"

namespace chess::graphics::svg {

// Rasterises an in-memory SVG with SDL_image (CPU surface, no renderer
// required) and uploads the pixels as a GL texture owned by a GLTexture.
// Requires a current GL context. Returns nullptr on failure.
inline std::shared_ptr<GLTexture> LoadSVGTextureFromMemory(
    const std::span<const std::uint8_t>& svg
) {
    SDL_IOStream* io = SDL_IOFromConstMem(svg.data(), svg.size_bytes());
    if (!io) {
        SDL_Log("Failed to create IOStream: %s", SDL_GetError());
        return nullptr;
    }

    SDL_Surface* raw_surface = IMG_LoadSVG_IO(io);
    SDL_CloseIO(io);

    if (!raw_surface) {
        SDL_Log("Failed to load SVG from memory: %s", SDL_GetError());
        return nullptr;
    }

    // Normalise to a known, tightly described RGBA8 layout before uploading.
    SDL_Surface* rgba_surface = SDL_ConvertSurface(raw_surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(raw_surface);

    if (!rgba_surface) {
        SDL_Log("Failed to convert SVG surface to RGBA32: %s", SDL_GetError());
        return nullptr;
    }

    std::shared_ptr<GLTexture> texture;
    try {
        texture = std::make_shared<GLTexture>(
            rgba_surface->pixels,
            rgba_surface->w,
            rgba_surface->h,
            rgba_surface->pitch / 4 // RGBA8: 4 bytes per pixel
        );
    } catch (...) {
        SDL_DestroySurface(rgba_surface);
        throw;
    }

    SDL_DestroySurface(rgba_surface);
    return texture;
}

} // namespace chess::graphics::svg
