#pragma once

#include <cstdint>
#include <array>
#include <stdexcept>
#include <span>
#include <memory>
#include <algorithm>

#include <SDL3/SDL.h>

#include "../RGBA.hpp"

static constexpr std::uint8_t kBoardAtlasBytes[] = {
    #embed "Board.rgba"
};

using BoardUInt8Span = std::span<const std::uint8_t>;
static constexpr BoardUInt8Span kBoardAtlas{kBoardAtlasBytes, sizeof(kBoardAtlasBytes)};

using BoardColorPaletteT = std::array<RGBA, 4>;

template <typename T>
concept IsBoardColorPalette = std::same_as<std::remove_cvref_t<T>, BoardColorPaletteT>;

constexpr BoardColorPaletteT kBasicBoardColorPalette {
    RGBA{ 234, 240, 216, 255 }, // Lightest
    RGBA{ 150, 162, 179, 255 }, // Light
    RGBA{ 89,   96, 112, 255 }, // Dark
    RGBA{ 31,   31,  41,  255 }  // Darkest
};

namespace board_detail {

consteval void CompileTimeAssertRGBASize(const std::size_t size) {
    if (size % 4 != 0) {
        throw "Pixel data length is not a multiple of 4 (RGBA)!";
    }
}

template <RGBA TargetRGBA>
consteval std::size_t CountPixelsMatchingRGBA(const BoardUInt8Span pixels) {
    CompileTimeAssertRGBASize(pixels.size());
    std::size_t count = 0;
    for (std::size_t it = 0; it < pixels.size(); it += 4) {
        if (pixels[it]     == TargetRGBA.red   &&
            pixels[it + 1] == TargetRGBA.green &&
            pixels[it + 2] == TargetRGBA.blue  &&
            pixels[it + 3] == TargetRGBA.alpha) {
            count = count + 1;
        }
    }
    return count;
}

template <RGBA TargetRGBA>
consteval auto FindPixelsMatchingRGBA(const BoardUInt8Span pixels) {
    constexpr std::size_t matching_pixel_count = CountPixelsMatchingRGBA<TargetRGBA>(kBoardAtlas);
    std::array<std::size_t, matching_pixel_count> pixel_locations{};
    std::size_t idx = 0;
    for (std::size_t it = 0; it < pixels.size(); it += 4) {
        if (pixels[it]     == TargetRGBA.red   &&
            pixels[it + 1] == TargetRGBA.green &&
            pixels[it + 2] == TargetRGBA.blue  &&
            pixels[it + 3] == TargetRGBA.alpha) {
            pixel_locations[idx] = it;
            idx = idx + 1;
        }
    }
    return pixel_locations;
}

} // namespace board_detail

template <BoardColorPaletteT Palette, const auto& Pixels>
struct BoardAtlasColorPaletteMask {
    using LightestT = decltype(board_detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels));
    using LightT    = decltype(board_detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels));
    using DarkT     = decltype(board_detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels));
    using DarkestT  = decltype(board_detail::FindPixelsMatchingRGBA<Palette[3]>(Pixels));

    LightestT lightest = board_detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels);
    LightT    light    = board_detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels);
    DarkT     dark     = board_detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels);
    DarkestT  darkest  = board_detail::FindPixelsMatchingRGBA<Palette[3]>(Pixels);
};

constexpr BoardAtlasColorPaletteMask<kBasicBoardColorPalette, kBoardAtlas> kBoardAtlasColorPaletteMask{};

template <typename BoardAtlasColorPaletteMaskT>
constexpr auto BoardPaletteSwap(const BoardAtlasColorPaletteMaskT& mask, BoardColorPaletteT palette) {
    std::array<std::uint8_t, kBoardAtlas.size()> atlas_copy{};
    std::ranges::copy(kBoardAtlas.begin(), kBoardAtlas.end(), atlas_copy.begin());

    for (const auto& idx : mask.lightest) {
        atlas_copy.at(idx) = palette[0].red;
        atlas_copy.at(idx + 1) = palette[0].green;
        atlas_copy.at(idx + 2) = palette[0].blue;
        atlas_copy.at(idx + 3) = palette[0].alpha;
    }
    for (const auto& idx : mask.light) {
        atlas_copy.at(idx) = palette[1].red;
        atlas_copy.at(idx + 1) = palette[1].green;
        atlas_copy.at(idx + 2) = palette[1].blue;
        atlas_copy.at(idx + 3) = palette[1].alpha;
    }
    for (const auto& idx : mask.dark) {
        atlas_copy.at(idx) = palette[2].red;
        atlas_copy.at(idx + 1) = palette[2].green;
        atlas_copy.at(idx + 2) = palette[2].blue;
        atlas_copy.at(idx + 3) = palette[2].alpha;
    }
    for (const auto& idx : mask.darkest) {
        atlas_copy.at(idx) = palette[3].red;
        atlas_copy.at(idx + 1) = palette[3].green;
        atlas_copy.at(idx + 2) = palette[3].blue;
        atlas_copy.at(idx + 3) = palette[3].alpha;
    }

    return atlas_copy;
}

struct BoardTextureView {
    SDL_Texture* texture{nullptr};
    SDL_FRect region{0.0f, 0.0f, 0.0f, 0.0f};
};

class PaletteSwappedBoard {
public:
    // Adjust dimensions to match Board.rgba (142x142 = 20164 pixels * 4 = 80656 bytes)
    static constexpr std::size_t kBoardWidth = 142;
    static constexpr std::size_t kBoardHeight = 142;
    static constexpr std::size_t kPitch = kBoardWidth * 4;

    PaletteSwappedBoard(
        const BoardColorPaletteT color_palette,
        std::shared_ptr<SDL_Renderer> renderer
    ) {
        const auto swapped_pixels = BoardPaletteSwap(kBoardAtlasColorPaletteMask, color_palette);
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            kBoardWidth,
            kBoardHeight,
            SDL_PIXELFORMAT_RGBA32,
            (void*)swapped_pixels.data(),
            kPitch
        );

        if (!surface) {
            throw std::runtime_error("Failed to create board surface");
        }

        board_texture_ = SDL_CreateTextureFromSurface(renderer.get(), surface);
        SDL_DestroySurface(surface);

        if (!board_texture_) {
            throw std::runtime_error("Failed to create SDL_Texture for board.");
        }
    }

    ~PaletteSwappedBoard() {
        if (board_texture_) {
            SDL_DestroyTexture(board_texture_);
        }
    }

    PaletteSwappedBoard(const PaletteSwappedBoard&) = delete;
    PaletteSwappedBoard& operator=(const PaletteSwappedBoard&) = delete;

    [[nodiscard]] SDL_Texture* GetBoardTexture() const { return board_texture_; }

    [[nodiscard]] BoardTextureView GetView() const { 
        return BoardTextureView{ board_texture_, SDL_FRect{ 0.0f, 0.0f, (float)kBoardWidth, (float)kBoardHeight } }; 
    }

private:
    SDL_Texture* board_texture_{nullptr};
};
