# SDL3 Migration Plan

## Objective
Migrate rendering backend from OpenGL/GLFW/GLM to SDL3 (`SDL_Window` + `SDL_Renderer`), utilizing `imgui` + SDL3 best practices. Keep existing custom graphics wrappers in `src/Graphics/` as reference, but replace their active usage. Integrate the custom `PaletteSwappedPieceAtlas` for piece rendering and create a similar `PaletteSwappedBoard` implementation.

## Architecture Context
- `src/Graphics/Pieces/PieceAtlas.hpp` provides a compile-time palette-swappable surface generator (`PaletteSwappedPieceAtlas`).
- `TextureResources` struct in `Application.h` will manage the lifetimes of `PaletteSwappedPieceAtlas` (white/black) and `PaletteSwappedBoard`.
- Use `Board.rgba` for generating the board surface/texture.

## Task Checklist

### Step 1: Graphics Directory Reference
- [ ] Retain `src/Graphics/` as a reference. Do not delete.
- [ ] Add `PaletteSwappedBoard` implementation (similar to `PaletteSwappedPieceAtlas`) to dynamically generate board textures from `Board.rgba`.

### Step 2: src/Application/Application.h
- [ ] Remove `#include <glm/glm.hpp>`, `"Graphics/Framebuffer.h"`, `"Graphics/SubTexture.h"`.
- [ ] Include `<SDL3/SDL.h>` and `"Graphics/Pieces/PieceAtlas.hpp"`.
- [ ] Define `TextureResources` struct:
  ```cpp
  struct TextureResources {
      std::shared_ptr<PaletteSwappedPieceAtlas> white_pieces{};
      std::shared_ptr<PaletteSwappedPieceAtlas> black_pieces{};
      std::shared_ptr<PaletteSwappedBoard> board{};
  };
  ```
- [ ] Replace `GLFWwindow* m_Window` with `SDL_Window* m_Window` and `std::shared_ptr<SDL_Renderer> m_Renderer`.
- [ ] Replace `glm::vec2` and `glm::vec4` with `SDL_FPoint`, `SDL_FRect`, and `SDL_Color`.
- [ ] Add `TextureResources m_TextureResources{};`.
- [ ] Replace `std::array<std::shared_ptr<SubTexture>, 12> m_ChessPieceSprites` with `m_TextureResources`.
- [ ] Replace `std::shared_ptr<Framebuffer> m_ChessViewport` with `SDL_Texture* m_BoardTargetTexture` (using `SDL_TEXTUREACCESS_TARGET`).
- [ ] Remove `m_CoordinateTransform`.

### Step 3: src/Application/Application.cpp
- [ ] Replace `#include <GLFW/glfw3.h>` and glad with `<SDL3/SDL.h>`.
- [ ] Constructor: Initialize SDL (`SDL_Init(SDL_INIT_VIDEO)`). Create `SDL_Window` and `SDL_Renderer`.
- [ ] Remove `gladLoadGLLoader` and `DebugContext::Init()`.
- [ ] Update ImGui initialization: `ImGui_ImplSDL3_InitForSDLRenderer` and `ImGui_ImplSDLRenderer3_Init`.
- [ ] Refactor main loop: Replace `glfwPollEvents()` with `SDL_PollEvent(&event)` loop. Pass events to `ImGui_ImplSDL3_ProcessEvent(&event)`. Handle `SDL_EVENT_QUIT`, window resizing, mouse, and key events.
- [ ] Refactor `Run()` to clear the renderer, render ImGui, render ImGui draw data via `ImGui_ImplSDLRenderer3_RenderDrawData`, and `SDL_RenderPresent`.
- [ ] In `Init()`: Instantiate `m_TextureResources.white_pieces`, `m_TextureResources.black_pieces`, and `m_TextureResources.board`.

### Step 4: src/Application/ChessPanel.cpp
- [ ] `RenderBoard()`: Bind `m_BoardTargetTexture` as target (`SDL_SetRenderTarget(m_Renderer, m_BoardTargetTexture)`).
- [ ] Draw the board using `m_TextureResources.board`.
- [ ] Draw legal move highlights using `SDL_SetRenderDrawBlendMode(SDL_BLENDMODE_BLEND)` and `SDL_RenderFillRect`.
- [ ] Draw pieces by retrieving `TextureView` from `m_TextureResources` and calling `SDL_RenderTexture(m_Renderer, view.texture, &view.region, &dstRect)`.
- [ ] Revert target: `SDL_SetRenderTarget(m_Renderer, nullptr)`.
- [ ] `RenderChessPanel()`: Pass `m_BoardTargetTexture` to `ImGui::ImageButton`.

### Step 5: Platform Adjustments
- [ ] `src/Application/Main.cpp`: Change native message box to use `SDL_ShowSimpleMessageBox`.
- [ ] `src/Platform/Windows/WindowsFileDialog.cpp`: Get `HWND` from SDL3 using `SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER)`.
