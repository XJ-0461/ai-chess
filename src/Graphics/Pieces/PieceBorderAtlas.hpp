#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "PieceAtlas.hpp"

// A border/background atlas for the piece sprites. Rather than scaling a
// silhouette up (which is resolution-dependent and looks wrong), the border is
// the piece shape *dilated by one pixel*: every transparent pixel that touches a
// real (non-transparent) piece pixel is added to the mask. Palette-swapping the
// mask to a single colour and drawing it behind the piece at the SAME size/rect
// yields a clean, palette-able 1px outline (the dilated ring peeks out around
// the piece; the interior is covered by the piece itself).

namespace border_detail {

inline constexpr std::size_t kWidth      = PaletteSwappedPieceAtlas::kTotalWidth;  // 96
inline constexpr std::size_t kHeight     = PaletteSwappedPieceAtlas::kPieceHeight; // 16
inline constexpr std::size_t kSpriteSize = PaletteSwappedPieceAtlas::kPieceWidth;  // 16
inline constexpr std::size_t kPixelCount = kWidth * kHeight;

// True for every pixel that is either part of a piece (non-transparent) or
// 8-adjacent to one. Dilation is clamped to each 16px sprite cell so a piece's
// border never bleeds into the neighbouring sprite.
consteval std::array<bool, kPixelCount> ComputeBorderGrid() {
    std::array<bool, kPixelCount> occupied{};
    for (std::size_t p = 0; p < kPixelCount; ++p) {
        occupied[p] = kPieceAtlas[p * 4 + 3] != 0; // alpha != 0
    }

    std::array<bool, kPixelCount> border{};
    for (std::size_t p = 0; p < kPixelCount; ++p) {
        if (!occupied[p]) {
            continue;
        }
        const std::size_t x = p % kWidth;
        const std::size_t y = p / kWidth;
        const std::size_t sprite = x / kSpriteSize;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const long nx = static_cast<long>(x) + dx;
                const long ny = static_cast<long>(y) + dy;
                if (nx < 0 || ny < 0 ||
                    nx >= static_cast<long>(kWidth) || ny >= static_cast<long>(kHeight)) {
                    continue;
                }
                if (static_cast<std::size_t>(nx) / kSpriteSize != sprite) {
                    continue; // stay within this sprite's 16px cell
                }
                border[static_cast<std::size_t>(ny) * kWidth + static_cast<std::size_t>(nx)] = true;
            }
        }
    }
    return border;
}

consteval std::size_t CountBorderPixels() {
    const auto border = ComputeBorderGrid();
    std::size_t count = 0;
    for (const bool set : border) {
        if (set) {
            ++count;
        }
    }
    return count;
}

consteval auto FindBorderPixels() {
    constexpr std::size_t count = CountBorderPixels();
    std::array<std::size_t, count> offsets{};
    const auto border = ComputeBorderGrid();
    std::size_t idx = 0;
    for (std::size_t p = 0; p < kPixelCount; ++p) {
        if (border[p]) {
            offsets[idx++] = p * 4; // byte offset, matching the colour masks
        }
    }
    return offsets;
}

} // namespace border_detail

// Byte offsets of all border (dilated piece) pixels, computed at compile time.
struct PieceAtlasBorderColorPaletteMask {
    using BorderT = decltype(border_detail::FindBorderPixels());
    BorderT border = border_detail::FindBorderPixels();
};

constexpr PieceAtlasBorderColorPaletteMask kPieceAtlasBorderColorPaletteMask{};

// Paints every masked (border) pixel a single colour over a transparent atlas.
constexpr auto PaletteSwap(const PieceAtlasBorderColorPaletteMask& mask, RGBA color) {
    std::array<std::uint8_t, kPieceAtlas.size()> atlas_copy{}; // zero-init -> transparent
    for (const auto& idx : mask.border) {
        atlas_copy[idx]     = color.red;
        atlas_copy[idx + 1] = color.green;
        atlas_copy[idx + 2] = color.blue;
        atlas_copy[idx + 3] = color.alpha;
    }
    return atlas_copy;
}

// A single-colour, palette-swappable border atlas. Mirrors the piece accessors
// of PaletteSwappedPieceAtlas so it can be drawn with the identical layout.
class PaletteSwappedPieceBorderAtlas {
public:
    static constexpr std::size_t kPieceWidth = PaletteSwappedPieceAtlas::kPieceWidth;
    static constexpr std::size_t kPieceHeight = PaletteSwappedPieceAtlas::kPieceHeight;
    static constexpr std::size_t kTotalWidth = PaletteSwappedPieceAtlas::kTotalWidth;

    explicit PaletteSwappedPieceBorderAtlas(const RGBA border_color)
        : border_color_(border_color) {}

    ~PaletteSwappedPieceBorderAtlas() {
        if (atlas_texture_ != 0) {
            glDeleteTextures(1, &atlas_texture_);
        }
    }

    PaletteSwappedPieceBorderAtlas(const PaletteSwappedPieceBorderAtlas&) = delete;
    PaletteSwappedPieceBorderAtlas& operator=(const PaletteSwappedPieceBorderAtlas&) = delete;

    // Requires a current GL context.
    void MakeTexture() {
        const auto swapped_pixels = PaletteSwap(kPieceAtlasBorderColorPaletteMask, border_color_);
        atlas_texture_ = chess::graphics::UploadRGBATexture(
            swapped_pixels.data(),
            kTotalWidth,
            kPieceHeight,
            kTotalWidth
        );
    }

    [[nodiscard]] ImTextureID GetAtlasTexture() const { return static_cast<ImTextureID>(atlas_texture_); }

    [[nodiscard]] TextureView GetPawnTexture() const   { return { GetAtlasTexture(), { 0.0f,  0.0f, 16.0f, 16.0f } }; }
    [[nodiscard]] TextureView GetKnightTexture() const { return { GetAtlasTexture(), { 16.0f, 0.0f, 16.0f, 16.0f } }; }
    [[nodiscard]] TextureView GetRookTexture() const   { return { GetAtlasTexture(), { 32.0f, 0.0f, 16.0f, 16.0f } }; }
    [[nodiscard]] TextureView GetBishopTexture() const { return { GetAtlasTexture(), { 48.0f, 0.0f, 16.0f, 16.0f } }; }
    [[nodiscard]] TextureView GetQueenTexture() const  { return { GetAtlasTexture(), { 64.0f, 0.0f, 16.0f, 16.0f } }; }
    [[nodiscard]] TextureView GetKingTexture() const   { return { GetAtlasTexture(), { 80.0f, 0.0f, 16.0f, 16.0f } }; }

private:
    RGBA border_color_{};
    GLuint atlas_texture_{0};
};
