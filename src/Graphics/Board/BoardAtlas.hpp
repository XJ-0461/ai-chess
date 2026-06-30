#pragma once

#include <cstdint>
#include <array>
#include <stdexcept>
#include <span>
#include <memory>
#include <algorithm>

#include "../RGBA.hpp"
#include "../GLTexture.hpp"

static constexpr std::uint8_t kBoardAtlasBytes[] = {
    #embed "Board.rgba"
};

using BoardUInt8Span = std::span<const std::uint8_t>;
static constexpr BoardUInt8Span kBoardAtlas{kBoardAtlasBytes, sizeof(kBoardAtlasBytes)};

using BoardColorPaletteT = std::array<RGBA, 3>;

template <typename T>
concept IsBoardColorPalette = std::same_as<std::remove_cvref_t<T>, BoardColorPaletteT>;

constexpr BoardColorPaletteT kStandardBoardColorPalette {
    RGBA{ 234, 240, 216, 255 }, // Light
    RGBA{ 89,   96, 112, 255 }, // Dark
    RGBA{ 150, 162, 179, 255 }  // Accent
};

constexpr BoardColorPaletteT kCreamBoardColorPalette {
    RGBA{ 234, 240, 216, 255 },
    RGBA{ 192, 196, 179, 255 },
    RGBA{ 234, 240, 216, 255 }
};

constexpr BoardColorPaletteT kDarkBoardColorPalette {
    RGBA{ 150, 162, 179, 255 },
    RGBA{ 89,   96, 112, 255 },
    RGBA{ 150, 162, 179, 255 }
};

constexpr BoardColorPaletteT kCreamBlueBoardColorPalette {
    RGBA{ 230, 234, 215, 255 },
    RGBA{ 69,   77,  95, 255 },
    RGBA{ 150, 162, 179, 255 }
};

constexpr BoardColorPaletteT kWoodBoardColorPalette {
    RGBA{ 226, 213, 161, 255 },
    RGBA{ 120,  79,  72, 255 },
    RGBA{ 226, 213, 161, 255 }
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
    using LightT    = decltype(board_detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels));
    using DarkT     = decltype(board_detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels));
    using AccentT   = decltype(board_detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels));

    LightT    light    = board_detail::FindPixelsMatchingRGBA<Palette[0]>(Pixels);
    DarkT     dark     = board_detail::FindPixelsMatchingRGBA<Palette[1]>(Pixels);
    AccentT   accent   = board_detail::FindPixelsMatchingRGBA<Palette[2]>(Pixels);
};

constexpr BoardAtlasColorPaletteMask<kStandardBoardColorPalette, kBoardAtlas> kBoardAtlasColorPaletteMask{};

template <typename BoardAtlasColorPaletteMaskT>
constexpr auto BoardPaletteSwap(const BoardAtlasColorPaletteMaskT& mask, BoardColorPaletteT palette) {
    std::array<std::uint8_t, kBoardAtlas.size()> atlas_copy{};
    std::ranges::copy(kBoardAtlas.begin(), kBoardAtlas.end(), atlas_copy.begin());

    for (const auto& idx : mask.light) {
        atlas_copy.at(idx) = palette[0].red;
        atlas_copy.at(idx + 1) = palette[0].green;
        atlas_copy.at(idx + 2) = palette[0].blue;
        atlas_copy.at(idx + 3) = palette[0].alpha;
    }
    for (const auto& idx : mask.dark) {
        atlas_copy.at(idx) = palette[1].red;
        atlas_copy.at(idx + 1) = palette[1].green;
        atlas_copy.at(idx + 2) = palette[1].blue;
        atlas_copy.at(idx + 3) = palette[1].alpha;
    }
    for (const auto& idx : mask.accent) {
        atlas_copy.at(idx) = palette[2].red;
        atlas_copy.at(idx + 1) = palette[2].green;
        atlas_copy.at(idx + 2) = palette[2].blue;
        atlas_copy.at(idx + 3) = palette[2].alpha;
    }

    return atlas_copy;
}

struct BoardTextureView {
    ImTextureID texture{0};
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

class PaletteSwappedBoardAtlas {
public:
    // Adjust dimensions to match Board.rgba (142x142 = 20164 pixels * 4 = 80656 bytes)
    static constexpr std::size_t kBoardWidth = 142;
    static constexpr std::size_t kBoardHeight = 142;
    static constexpr std::size_t kPitch = kBoardWidth * 4;

    // Performs the palette swap and uploads the result as a GL texture.
    // Requires a current GL context.
    explicit PaletteSwappedBoardAtlas(const BoardColorPaletteT color_palette) {
        const auto swapped_pixels = BoardPaletteSwap(kBoardAtlasColorPaletteMask, color_palette);
        board_texture_ = chess::graphics::UploadRGBATexture(
            swapped_pixels.data(),
            static_cast<int>(kBoardWidth),
            static_cast<int>(kBoardHeight),
            static_cast<int>(kBoardWidth)
        );
    }

    ~PaletteSwappedBoardAtlas() {
        if (board_texture_ != 0) {
            glDeleteTextures(1, &board_texture_);
        }
    }

    PaletteSwappedBoardAtlas(const PaletteSwappedBoardAtlas&) = delete;
    PaletteSwappedBoardAtlas& operator=(const PaletteSwappedBoardAtlas&) = delete;

    [[nodiscard]] ImTextureID GetBoardTexture() const { return static_cast<ImTextureID>(board_texture_); }

    [[nodiscard]] BoardTextureView GetView() const {
        return BoardTextureView{ GetBoardTexture(), 0.0f, 0.0f, (float)kBoardWidth, (float)kBoardHeight };
    }

private:
    GLuint board_texture_{0};
};
