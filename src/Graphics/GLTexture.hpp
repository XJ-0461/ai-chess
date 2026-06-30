#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>

// SDL ships the OpenGL 1.1 prototypes/constants we need (glGenTextures,
// glTexImage2D, glDeleteTextures, ...). The Dear ImGui OpenGL3 renderer
// backend carries its own loader for the modern functions it uses, so we do
// not pull in a separate GL loader here.
#include <SDL3/SDL_opengl.h>

#include <imgui.h>

namespace chess::graphics {

// Uploads tightly addressable RGBA8 pixel data to a freshly allocated GL
// texture and returns its name. `row_length_pixels` is the number of pixels
// per source row (pass the width for tightly packed data, or the surface
// pitch / 4 when the source has padding). Requires a current GL context.
[[nodiscard]] inline GLuint UploadRGBATexture(
    const void* pixels,
    const int width,
    const int height,
    const int row_length_pixels
) {
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    if (texture_id == 0) {
        throw std::runtime_error("Failed to allocate GL texture (glGenTextures returned 0).");
    }

    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    GLint previous_row_length = 0;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previous_row_length);
    GLint previous_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_alignment);

    glBindTexture(GL_TEXTURE_2D, texture_id);

    // 1. Set pixel storage mode
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length_pixels);

    // 2. Allocate
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // 3. Set parameters AFTER storage is allocated
    // Nearest-neighbour: the board/piece atlases are small baked pixel art that
    // is heavily upscaled, so linear filtering makes them blurry.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 4. CRITICAL: Explicitly disable mipmapping for Core Profile completion
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // 5. Restore the global pixel-store and binding state we touched.
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, previous_row_length);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));

    return texture_id;
}

// RAII owner of a GL texture for shared-ownership use cases (e.g. icons held
// in TextureResources and rendered by many bubbles). Destruction requires a
// current GL context, so owners must release these before tearing the context
// down.
class GLTexture {
public:
    GLTexture(const void* pixels, const int width, const int height, const int row_length_pixels)
        : texture_id_(UploadRGBATexture(pixels, width, height, row_length_pixels)),
          width_(width),
          height_(height) {}

    ~GLTexture() {
        if (texture_id_ != 0) {
            glDeleteTextures(1, &texture_id_);
        }
    }

    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    [[nodiscard]] ImTextureID Id() const { return static_cast<ImTextureID>(texture_id_); }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    GLuint texture_id_{0};
    int width_{0};
    int height_{0};
};

// RAII guard that switches Dear ImGui's OpenGL3 backend to nearest-neighbour
// texture sampling for the lifetime of the scope, then restores the backend's
// default (linear) on destruction.
//
// Why this is needed: the ImGui 1.92+ GL3 backend binds a GL_LINEAR *sampler
// object* to texture unit 0 every frame. A bound sampler object overrides the
// per-texture GL_TEXTURE_MIN/MAG_FILTER we set in UploadRGBATexture (and is
// invisible to glGetTexParameteriv), so pixel-art textures get blurred when
// upscaled. There is no per-texture way to opt out — samplers are bound per
// texture *unit* — so the only hook is the backend's per-draw sampler callback.
// Wrap the AddImage() calls for pixel-art textures in one of these; everything
// drawn outside the scope keeps the default linear filtering (e.g. AA text).
//
// Restoring on scope exit is mandatory, not cosmetic: the sampler binding is GL
// state that persists across the whole RenderDrawData pass, so leaving it on
// nearest would affect every panel drawn after this one in the same frame.
class NearestSamplerScope {
public:
    explicit NearestSamplerScope(ImDrawList* draw_list) : draw_list_(draw_list) {
        draw_list_->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    }

    ~NearestSamplerScope() {
        draw_list_->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    }

    NearestSamplerScope(const NearestSamplerScope&) = delete;
    NearestSamplerScope& operator=(const NearestSamplerScope&) = delete;
    NearestSamplerScope(NearestSamplerScope&&) = delete;
    NearestSamplerScope& operator=(NearestSamplerScope&&) = delete;

private:
    ImDrawList* draw_list_;
};

// Convenience: resolve an optional icon owner to an ImTextureID, yielding the
// null texture (0) when the icon is absent.
[[nodiscard]] inline ImTextureID IconId(const std::shared_ptr<GLTexture>& icon) {
    return icon ? icon->Id() : ImTextureID{0};
}

} // namespace chess::graphics
