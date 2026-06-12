# Central Chessboard Panel Composition

This panel is responsible for compositing/drawing the game state to the `CentralChessboardPanel` and handling board interaction.

## Objectives
- **Board State:** `CentralChessboardPanel` receives `std::shared_ptr<Board>` and interaction state (`m_SelectedPiece`, `m_LegalMoves`, `m_IsHoldingPiece`) from `Application`.
- **Layout & Sizing:**
  - Centered in the panel.
  - Maintain square aspect ratio (1:1).
  - Scale to maximum possible size within the panel viewport.
  - Track empty space above/below or left/right.
- **Rendering (ImDrawList):**
  - Draw the board texture.
  - Draw legal move highlights and selected square highlights on top of the board.
  - Draw pieces composited on top of the chessboard squares.
  - Draw the dragged piece at the mouse cursor.
- **Interaction (Mouse):**
  - Replace legacy OpenGL matrix math.
  - Panel computes which square is hovered based on `ImGui::GetMousePos()` relative to the board's screen bounding box.
  - Panel handles clicks/drags and updates interaction state (passed from Application).

## Implementation Details
- Use `ImGui::GetWindowDrawList()` for direct rendering.
- `BoardAtlas` and `PieceAtlas` provide UV coordinates for textures.
- Interaction logic migrated from `Application::OnMouseButton` to `CentralChessboardPanel::Render`.
