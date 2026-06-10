#pragma once

#include <array>
#include <stdexcept>
#include <string_view>

#include <SDL3/SDL.h>

struct RGBA {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

static constexpr std::uint8_t kPieceAtlasBytes[] = {
    #embed "Pieces.rgba"
};

using UInt8Span = std::span<const std::uint8_t>;
static constexpr UInt8Span kPieceAtlas{kPieceAtlasBytes, sizeof(kPieceAtlasBytes)};

using PieceColorPaletteT = std::array<RGBA, 4>;

template <typename T>
concept IsPieceColorPalette = std::same_as<std::remove_cvref_t<T>, PieceColorPaletteT>;

// lightest -> light -> dark -> darkest
constexpr PieceColorPaletteT kBasicWhiteColorPalette {
    RGBA{ 234, 240, 216, 255 },
    RGBA{ 150, 162, 179, 255 },
    RGBA{ 89,   96, 112, 255 },
    RGBA{ 31,   31,  41,  255 }
};

constexpr PieceColorPaletteT kBasicBlackColorPalette {
    RGBA{ 150, 162, 179, 255 },
    RGBA{ 89,  96,  112, 255 },
    RGBA{ 65,  58,  66,  255 },
    RGBA{ 31,  31,  41,  255 }
};

namespace detail {

// Helper function to force a compile error if asset size is invalid
consteval void CompileTimeAssertRGBASize(const std::size_t size) {
    if (size % 4 != 0) {
        // Trigger a clean compilation error
        throw "Pixel data length is not a multiple of 4 (RGBA)!";
    }
}

template <RGBA TargetRGBA>
consteval std::size_t CountPixelsMatchingRGBA(
    const UInt8Span pixels
) {
    CompileTimeAssertRGBASize(pixels.size());
    std::size_t count = 0;
    for (std::size_t it = 0; it < pixels.size(); it += 4) {
        if (pixels[it]     == TargetRGBA.red   &&
            pixels[it + 1] == TargetRGBA.green &&
            pixels[it + 2] == TargetRGBA.blue  &&
            pixels[it + 3] == TargetRGBA.alpha
        ) {
            count = count + 1;
        }
    }
    return count;
}

template <RGBA TargetRGBA>
consteval auto FindPixelsMatchingRGBA(
    const UInt8Span pixels
) {

    constexpr std::size_t matching_pixel_count = CountPixelsMatchingRGBA<TargetRGBA>(kPieceAtlas);
    std::array<std::size_t, matching_pixel_count> pixel_locations{};
    std::size_t idx = 0;
    for (std::size_t it = 0; it < pixels.size(); it += 4) {
        if (pixels[it]     == TargetRGBA.red   &&
            pixels[it + 1] == TargetRGBA.green &&
            pixels[it + 2] == TargetRGBA.blue  &&
            pixels[it + 3] == TargetRGBA.alpha
        ) {
            pixel_locations[idx] = it;
            idx = idx + 1;
        }
    }
    return pixel_locations;
}

} // namespace detail

template <PieceColorPaletteT Palette, const auto& Pixels>
struct PieceAtlasColorPaletteMask {

    // Auto-compute types cleanly inside the struct definition
    using LightestT = decltype(detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels));
    using LightT    = decltype(detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels));
    using DarkT     = decltype(detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels));
    using DarkestT  = decltype(detail::FindPixelsMatchingRGBA<Palette[3]>(Pixels));

    // Instantiated offsets fields
    LightestT lightest = detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels);
    LightT    light    = detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels);
    DarkT     dark     = detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels);
    DarkestT  darkest  = detail::FindPixelsMatchingRGBA<Palette[3]>(Pixels);
};

constexpr PieceAtlasColorPaletteMask<kBasicWhiteColorPalette, kPieceAtlas> kPieceAtlasColorPaletteMask{};

template <typename PieceAtlasColorPaletteMaskT>
constexpr auto PaletteSwap(const PieceAtlasColorPaletteMaskT& mask, PieceColorPaletteT palette) {

    std::array<std::uint8_t, kPieceAtlas.size()> atlas_copy{};
    std::ranges::copy(kPieceAtlas.begin(), kPieceAtlas.end(), atlas_copy.begin());

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

struct TextureView {
    SDL_Texture* texture{nullptr};
    SDL_FRect region{0.0f, 0.0f, 0.0f, 0.0f};
};

class PaletteSwappedPieceAtlas {
public:

    static constexpr std::size_t kPieceWidth = 16;
    static constexpr std::size_t kPieceHeight = 16;
    static constexpr std::size_t kTotalWidth = kPieceWidth * 6 /* piece count */;
    static constexpr std::size_t kPitch = kTotalWidth * 4 /* RGBA */;

    enum class PieceType {
        Pawn = 0,
        Knight = 1,
        Rook = 2,
        Bishop = 3,
        Queen = 4,
        King = 5
    };

    PaletteSwappedPieceAtlas(
        const PieceColorPaletteT color_palette,
        std::shared_ptr<SDL_Renderer> renderer
    ) {

        const auto swapped_pixels = PaletteSwap(kPieceAtlasColorPaletteMask, color_palette);
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            kTotalWidth,
            kPieceHeight,
            SDL_PIXELFORMAT_RGBA32,
            (void*)swapped_pixels.data(),
            kPitch
        );

        if (!surface) {
            throw std::runtime_error("Failed to create texture");
        }

        atlas_texture_ = SDL_CreateTextureFromSurface(renderer.get(), surface);
        SDL_DestroySurface(surface);

        if (!atlas_texture_) {
            throw std::runtime_error("Failed to create SDL_Texture from palette-swapped surface.");
        }

    }

    ~PaletteSwappedPieceAtlas() {
        if (atlas_texture_) {
            SDL_DestroyTexture(atlas_texture_);
        }
    }

    PaletteSwappedPieceAtlas(const PaletteSwappedPieceAtlas&) = delete;
    PaletteSwappedPieceAtlas& operator=(const PaletteSwappedPieceAtlas&) = delete;

    // Direct access to the raw texture resource
    [[nodiscard]] SDL_Texture* GetAtlasTexture() const { return atlas_texture_; }

    // --- Piece Accessors returning exact SDL3 source clip region coordinates ---
    [[nodiscard]] TextureView GetPawnTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 0.0f, 0.0f, 16.0f, 16.0f } }; 
    }
    [[nodiscard]] TextureView GetKnightTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 16.0f, 0.0f, 16.0f, 16.0f } }; 
    }
    [[nodiscard]] TextureView GetRookTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 32.0f, 0.0f, 16.0f, 16.0f } }; 
    }
    [[nodiscard]] TextureView GetBishopTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 48.0f, 0.0f, 16.0f, 16.0f } }; 
    }
    [[nodiscard]] TextureView GetQueenTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 64.0f, 0.0f, 16.0f, 16.0f } }; 
    }
    [[nodiscard]] TextureView GetKingTexture() const { 
        return TextureView{ atlas_texture_, SDL_FRect{ 80.0f, 0.0f, 16.0f, 16.0f } }; 
    }

private:

    SDL_Texture* atlas_texture_{nullptr};

};
