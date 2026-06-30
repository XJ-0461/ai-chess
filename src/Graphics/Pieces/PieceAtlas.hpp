#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>

#include <memory>

#include "../RGBA.hpp"
#include "../GLTexture.hpp"

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

constexpr PieceColorPaletteT kNvidiaWhiteColorPalette {
    RGBA{ 255, 255, 255, 255 },
    RGBA{ 240, 255, 240, 255 },
    RGBA{ 118,   185, 0, 255 },
    RGBA{ 31,   31,  41,  255 }
};

constexpr PieceColorPaletteT kNvidiaBlackColorPalette {
    RGBA{ 240, 255, 240, 255 },
    RGBA{ 118,  185,  0, 255 },
    RGBA{ 77,  122,  0,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kQwenWhiteColorPalette {
    RGBA{ 240, 237, 255, 255 },
    RGBA{ 208, 200, 255, 255 },
    RGBA{ 138, 122, 255, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kQwenBlackColorPalette {
    RGBA{ 208, 200, 255, 255 },
    RGBA{ 138, 122, 255, 255 },
    RGBA{ 90,  77,  184, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kClaudeWhiteColorPalette {
    RGBA{ 255, 250, 245, 255 },
    RGBA{ 245, 235, 224, 255 },
    RGBA{ 217, 119, 87,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kClaudeBlackColorPalette {
    RGBA{ 245, 235, 224, 255 },
    RGBA{ 217, 119, 87,  255 },
    RGBA{ 150, 78,  57,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kGeminiWhiteColorPalette {
    RGBA{ 238, 244, 255, 255 },
    RGBA{ 192, 217, 255, 255 },
    RGBA{ 71,  150, 227, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kGeminiBlackColorPalette {
    RGBA{ 192, 217, 255, 255 },
    RGBA{ 71,  150, 227, 255 },
    RGBA{ 42,  90,  138, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMistralWhiteColorPalette {
    RGBA{ 255, 242, 234, 255 },
    RGBA{ 255, 216, 192, 255 },
    RGBA{ 255, 102, 51,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMistralBlackColorPalette {
    RGBA{ 255, 216, 192, 255 },
    RGBA{ 255, 102, 51,  255 },
    RGBA{ 179, 71,  36,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kPoolsideWhiteColorPalette {
    RGBA{ 158, 150, 255, 255 },
    RGBA{ 65,  55,  255, 255 },
    RGBA{ 37,  25,  255, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kPoolsideBlackColorPalette {
    RGBA{ 65,  55,  255, 255 },
    RGBA{ 37,  25,  255, 255 },
    RGBA{ 26,  18,  179, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kOpenrouterWhiteColorPalette {
    RGBA{ 199, 209, 224, 255 },
    RGBA{ 150, 162, 179, 255 },
    RGBA{ 123, 136, 153, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kOpenrouterBlackColorPalette {
    RGBA{ 150, 162, 179, 255 },
    RGBA{ 123, 136, 153, 255 },
    RGBA{ 90,  99,  112, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kKimiWhiteColorPalette {
    RGBA{ 128, 200, 255, 255 },
    RGBA{ 0,   145, 255, 255 },
    RGBA{ 0,   122, 204, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kKimiBlackColorPalette {
    RGBA{ 0,   145, 255, 255 },
    RGBA{ 0,   122, 204, 255 },
    RGBA{ 0,   90,  153, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMinimaxWhiteColorPalette {
    RGBA{ 255, 166, 181, 255 },
    RGBA{ 255, 77,  109, 255 },
    RGBA{ 230, 0,   69,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMinimaxBlackColorPalette {
    RGBA{ 255, 77,  109, 255 },
    RGBA{ 230, 0,   69,  255 },
    RGBA{ 179, 0,   54,  255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMicrosoftWhiteColorPalette {
    RGBA{ 255, 185, 0,   255 },
    RGBA{ 242, 80,  34,  255 },
    RGBA{ 127, 186, 0,   255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMicrosoftBlackColorPalette {
    RGBA{ 242, 80,  34,  255 },
    RGBA{ 127, 186, 0,   255 },
    RGBA{ 0,   164, 239, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMetaWhiteColorPalette {
    RGBA{ 235, 245, 255, 255 },
    RGBA{ 0,   129, 255, 255 },
    RGBA{ 6,   104, 225, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kMetaBlackColorPalette {
    RGBA{ 0,   129, 255, 255 },
    RGBA{ 6,   104, 225, 255 },
    RGBA{ 5,   77,  167, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kDeepseekWhiteColorPalette {
    RGBA{ 240, 242, 255, 255 },
    RGBA{ 77,  107, 255, 255 },
    RGBA{ 61,  80,  255, 255 },
    RGBA{ 31,  31,  41,  255 }
};

constexpr PieceColorPaletteT kDeepseekBlackColorPalette {
    RGBA{ 77,  107, 255, 255 },
    RGBA{ 61,  80,  255, 255 },
    RGBA{ 45,  58,  191, 255 },
    RGBA{ 31,  31,  41,  255 }
};

struct ModelProviderColorPaletteDetails {
    std::string_view provider;
    PieceColorPaletteT white_palette;
    PieceColorPaletteT black_palette;
};

static constexpr std::array<ModelProviderColorPaletteDetails, 12> kModelProviderColorPalettes {
    ModelProviderColorPaletteDetails{ "nvidia",     kNvidiaWhiteColorPalette,     kNvidiaBlackColorPalette },
    ModelProviderColorPaletteDetails{ "qwen",       kQwenWhiteColorPalette,       kQwenBlackColorPalette },
    ModelProviderColorPaletteDetails{ "claude",     kClaudeWhiteColorPalette,     kClaudeBlackColorPalette },
    ModelProviderColorPaletteDetails{ "gemini",     kGeminiWhiteColorPalette,     kGeminiBlackColorPalette },
    ModelProviderColorPaletteDetails{ "mistral",    kMistralWhiteColorPalette,    kMistralBlackColorPalette },
    ModelProviderColorPaletteDetails{ "poolside",   kPoolsideWhiteColorPalette,   kPoolsideBlackColorPalette },
    ModelProviderColorPaletteDetails{ "openrouter", kOpenrouterWhiteColorPalette, kOpenrouterBlackColorPalette },
    ModelProviderColorPaletteDetails{ "kimi",       kKimiWhiteColorPalette,       kKimiBlackColorPalette },
    ModelProviderColorPaletteDetails{ "minimax",    kMinimaxWhiteColorPalette,    kMinimaxBlackColorPalette },
    ModelProviderColorPaletteDetails{ "microsoft",  kMicrosoftWhiteColorPalette,  kMicrosoftBlackColorPalette },
    ModelProviderColorPaletteDetails{ "meta",       kMetaWhiteColorPalette,       kMetaBlackColorPalette },
    ModelProviderColorPaletteDetails{ "deepseek",   kDeepseekWhiteColorPalette,    kDeepseekBlackColorPalette }
};

// if the `str` (lowcase) contains any of the .provider keywords, then return that ColorPaletteDetails object
inline std::optional<ModelProviderColorPaletteDetails> PaletteDetailsFromString(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    for (const auto& palette_details : kModelProviderColorPalettes) {
        if (lowerStr.find(palette_details.provider) != std::string::npos) {
            return palette_details;
        }
    }
    return std::nullopt;
}

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

// A region of an atlas texture. `texture` is an ImTextureID (a GL texture
// name) understood directly by the Dear ImGui OpenGL3 backend; `region` is in
// atlas pixel coordinates and consumers derive UVs from it.
struct TextureViewRegion {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

struct TextureView {
    ImTextureID texture{0};
    TextureViewRegion region{0.0f, 0.0f, 0.0f, 0.0f};
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

    explicit PaletteSwappedPieceAtlas(
        const PieceColorPaletteT color_palette
    ) : color_palette_(std::move(color_palette)) {}

    ~PaletteSwappedPieceAtlas() {
        if (atlas_texture_ != 0) {
            glDeleteTextures(1, &atlas_texture_);
        }
    }

    PaletteSwappedPieceAtlas(const PaletteSwappedPieceAtlas&) = delete;
    PaletteSwappedPieceAtlas& operator=(const PaletteSwappedPieceAtlas&) = delete;

    // Performs the palette swap and uploads the result as a GL texture.
    // Requires a current GL context.
    void MakeTexture() {
        const auto swapped_pixels = PaletteSwap(kPieceAtlasColorPaletteMask, color_palette_);
        atlas_texture_ = chess::graphics::UploadRGBATexture(
            swapped_pixels.data(),
            static_cast<int>(kTotalWidth),
            static_cast<int>(kPieceHeight),
            static_cast<int>(kTotalWidth)
        );
    }

    // Direct access to the raw texture resource (a GL texture name).
    [[nodiscard]] ImTextureID GetAtlasTexture() const { return static_cast<ImTextureID>(atlas_texture_); }

    [[nodiscard]] PieceColorPaletteT GetColorPalette() const { return color_palette_; }

    // --- Piece Accessors returning exact atlas source clip region coordinates ---
    [[nodiscard]] TextureView GetPawnTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 0.0f, 0.0f, 16.0f, 16.0f } };
    }

    [[nodiscard]] TextureView GetKnightTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 16.0f, 0.0f, 16.0f, 16.0f } };
    }

    [[nodiscard]] TextureView GetRookTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 32.0f, 0.0f, 16.0f, 16.0f } };
    }

    [[nodiscard]] TextureView GetBishopTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 48.0f, 0.0f, 16.0f, 16.0f } };
    }

    [[nodiscard]] TextureView GetQueenTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 64.0f, 0.0f, 16.0f, 16.0f } };
    }

    [[nodiscard]] TextureView GetKingTexture() const {
        return TextureView{ GetAtlasTexture(), TextureViewRegion{ 80.0f, 0.0f, 16.0f, 16.0f } };
    }

private:

    PieceColorPaletteT color_palette_{};
    GLuint atlas_texture_{0};

};
