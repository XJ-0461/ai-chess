# Command Handling

This chess game application has a dedicated API layer for programmatic control over game concerns.
This includes configuring providers, starting games, opening windows, querying game state, etc.

## Core Directives & Standards
1. **Style & Conventions:**
    - Always use `{ ... }` for `if/then/else`.
    - Use `const` variables consistently and whenever possible.
    - Global/constexpr constants must be `PascalCase` prefixed with `k` (e.g., `kSomethingLikeThis`).
    - Local variables must be `lower_snake_case`.
    - Private member variables must be `lower_snake_case` with a trailing `_` (e.g., `something_like_this_`).
    - Prefer `std::unique_ptr`/`std::shared_ptr` (with custom deleters) over raw pointers.
    - Utilize C++23 best practices and `constexpr` wherever possible.
    - Use appropriate namespacing strictly following Domain-Driven Design.

2. **Architecture:**
    - Adhere strictly to **Domain-Driven Design (DDD)** principles.
    - Employ **SObjectizer** best practices. Use the actor model effectively.
    - Decouple the "Game" state from the view/interaction state.

3. **Interaction:**
    - Commands follow request-response pattern with HTTP-style response codes (200 OK, 400 user error, 500 server error).
    - Caller is not required to handle responses.

---

## Current Architecture

### Command Entry Points

1. **Startup Command File** (`--commands <filepath>`)
   - Parsed during `Application::Init()`
   - Executed sequentially without replies

2. **ZMQ Command Server** (`--command-server-endpoint <endpoint>`)
   - ZMQ ROUTER socket for async request/reply
   - JSON requests with `id` correlation for response routing

3. **Internal UI**
   - Commands from ConfigureGameWindow sent to executor

### Command Execution Pipeline

```
Application::Init()
├── IntroduceExternalCommandExecutor() [SObjectizer actor]
└── Setup CommandTargetCallbacks:
    ├── configure_provider → Application::ConfigureProvider()
    ├── create_game → Application::CreateGame()
    └── find_game → Application::FindGame()

Command → so_5::send<ExecuteCommand>(executor, command)
       → ExternalCommandExecutor::OnExecuteCommand()
       → std::visit dispatches to Execute(CommandType&)
       → Invokes appropriate callback
```

### Key Files

| File | Purpose |
|------|---------|
| `src/Application/Command/Command.hpp` | Command variant type definitions |
| `src/Application/Command/CommandFile.cpp` | JSON parsing from file |
| `src/Application/Command/ExternalCommandExecutor.hpp` | SObjectizer actor that executes commands |
| `src/Application/Application.cpp` | Callbacks and command wiring |
| `src/Utility/ZMQCommandServer.hpp` | ZMQ ROUTER server for external control |

---

## Current Command Types

### 1. `configure_provider`
Configures a model provider with credentials.

```json
{
  "type": "configure_provider",
  "detail": {
    "name": "openrouter",
    "kind": "OpenRouter",      // or "AWSBedrock"
    "api_key": "..."           // OpenRouter
    // OR for AWSBedrock:
    // "access_key": "...", "secret_key": "...", "region": "us-east-1"
  }
}
```

**Handler:** `Application::ConfigureProvider()` - adds to `m_ProviderStore.providers`

### 2. `configure_game`
Configures a game with agents and settings.

```json
{
  "type": "configure_game",
  "detail": {
    "game_id": "match",
    "white": {
      "endpoint": "tcp://127.0.0.1:5555",
      "provider": "OpenRouter",
      "model_id": "anthropic/claude-3.5-sonnet"
    },
    "black": {
      "endpoint": "tcp://127.0.0.1:5556",
      "provider": "OpenRouter",
      "model_id": "openai/gpt-4o"
    },
    "enable_quip": true,
    "enable_draw_offer": true,
    "enable_resignation": true,
    "retrospective_turn_count": 0
  }
}
```

**Handler:** `Application::CreateGame()` - creates `GameContext`, introduces `GameOrchestrator`, waits for Ready phase

### 3. `start_game`
Starts a configured game.

```json
{
  "type": "start_game",
  "detail": { "game_id": "match" }
}
```

**Handler:** Sends `StartGame` message to orchestrator, waits for InProgress phase

### 4. `query_match_result`
Queries the result of a concluded game.

```json
{
  "type": "query_match_result",
  "detail": { "game_id": "match" }
}
```

**Response:**
```json
{
  "id": "<request_id>",
  "outcome": "checkmate",
  "winner": "white",
  "cause": "checkmate",
  "final_fen": "..."
}
```

---

## New Command: `open_spectator_view`

### Purpose
Opens a spectator view window for an existing game with configurable theme and window settings.

### Existing Infrastructure
- `GameView` class exists at `src/Application/GameView.h`
- `GameViewTheme` enum: `Default`, `HighContrast`
- `Application::OpenSpectatorView(game_id, theme)` method exists
- Views are stored in `m_GameViews` vector and rendered each frame

### JSON Format
```json
{
  "type": "open_spectator_view",
  "detail": {
    "game_id": "match",
    "game_view_theme": "default",
    "window_configuration": {
      "type": "floating",
      "window_size": {
        "width": 1920,
        "height": 1080
      },
      "window_position": {
        "x": 0,
        "y": 0
      }
    }
  }
}
```

### Implementation Plan

#### Step 1: Add Command Struct in `Command.hpp`

```cpp
struct WindowSize {
    std::uint32_t width{1920};
    std::uint32_t height{1080};
};

struct WindowPosition {
    std::int32_t x{0};
    std::int32_t y{0};
};

struct WindowConfiguration {
    std::string type{"floating"};  // "floating", "fullscreen", etc.
    WindowSize size{};
    WindowPosition position{};
};

struct OpenSpectatorViewCommand {
    std::string game_id{};
    std::string game_view_theme{"default"};  // "default" or "high_contrast"
    WindowConfiguration window_configuration{};
};

// Add to variant:
using Command = std::variant<
    ConfigureProviderCommand,
    ConfigureGameCommand,
    StartGameCommand,
    QueryMatchResultCommand,
    OpenSpectatorViewCommand  // NEW
>;
```

#### Step 2: Add Parser in `CommandFile.cpp`

```cpp
OpenSpectatorViewCommand ParseOpenSpectatorView(const nlohmann::json& detail) {
    OpenSpectatorViewCommand cmd;
    cmd.game_id = detail.at("game_id").get<std::string>();
    cmd.game_view_theme = detail.value("game_view_theme", "default");

    if (detail.contains("window_configuration")) {
        const auto& wc = detail.at("window_configuration");
        cmd.window_configuration.type = wc.value("type", "floating");
        if (wc.contains("window_size")) {
            cmd.window_configuration.size.width = wc.at("window_size").value("width", 1920);
            cmd.window_configuration.size.height = wc.at("window_size").value("height", 1080);
        }
        if (wc.contains("window_position")) {
            cmd.window_configuration.position.x = wc.at("window_position").value("x", 0);
            cmd.window_configuration.position.y = wc.at("window_position").value("y", 0);
        }
    }
    return cmd;
}
```

Add case in `LoadCommandsFromFile()`:
```cpp
} else if (type == "open_spectator_view") {
    commands.push_back(ParseOpenSpectatorView(detail));
}
```

#### Step 3: Add Callback in `ExternalCommandExecutor.hpp`

```cpp
struct CommandTargetCallbacks {
    // existing...
    std::function<void(const OpenSpectatorViewCommand&)> open_spectator_view;
};
```

#### Step 4: Add Execute Handler in `ExternalCommandExecutor.hpp`

```cpp
void Execute(const OpenSpectatorViewCommand& command) {
    if (callbacks_.open_spectator_view) {
        callbacks_.open_spectator_view(command);
    }
}
```

#### Step 5: Wire Callback in `Application.cpp`

In `Application::Init()` where callbacks are set up:
```cpp
callbacks.open_spectator_view = [this](const OpenSpectatorViewCommand& cmd) {
    // Parse theme string to enum
    const auto theme = (cmd.game_view_theme == "high_contrast")
        ? chess::application::GameViewTheme::HighContrast
        : chess::application::GameViewTheme::Default;

    // Note: window_configuration is currently ignored - ImGui handles window sizing
    // Future: could set SDL window size/position for fullscreen mode
    OpenSpectatorView(cmd.game_id, theme);
};
```

#### Step 6: Update Script

Update `scripts/start_chess_match.sh` JSON generation:
```bash
cat > "$STARTUP_COMMANDS_FILE" << EOF
{
  "commands": [
    {
      "type": "configure_provider",
      "detail": { "name": "openrouter", "kind": "OpenRouter", "api_key": "$OPENROUTER_API_KEY" }
    },
    {
      "type": "configure_game",
      "detail": {
        "game_id": "$GAME_ID",
        "white": { "endpoint": "tcp://127.0.0.1:$WHITE_PORT", "provider": "OpenRouter", "model_id": "$WHITE_MODEL" },
        "black": { "endpoint": "tcp://127.0.0.1:$BLACK_PORT", "provider": "OpenRouter", "model_id": "$BLACK_MODEL" },
        "enable_quip": true,
        "enable_draw_offer": true,
        "enable_resignation": true,
        "retrospective_turn_count": $RETROSPECTIVE_ROUNDS
      }
    },
    {
      "type": "open_spectator_view",
      "detail": {
        "game_id": "$GAME_ID",
        "game_view_theme": "default",
        "window_configuration": {
          "type": "floating",
          "window_size": { "width": 1920, "height": 1080 },
          "window_position": { "x": 0, "y": 0 }
        }
      }
    },
    {
      "type": "start_game",
      "detail": { "game_id": "$GAME_ID" }
    }
  ]
}
EOF
```

### Files to Modify

1. `src/Application/Command/Command.hpp` - Add structs and variant
2. `src/Application/Command/CommandFile.cpp` - Add parser and case
3. `src/Application/Command/ExternalCommandExecutor.hpp` - Add callback and Execute handler
4. `src/Application/Application.cpp` - Wire callback (calls existing `OpenSpectatorView`)
5. `scripts/start_chess_match.sh` - Update JSON command structure

### Note on Window Configuration

The `window_configuration` field is parsed but currently ignored. The existing `GameView` uses ImGui's window management. Future enhancements could:
- Use SDL_SetWindowSize/Position for the main window in fullscreen mode
- Create separate SDL windows for floating views
- Use ImGui viewports for multi-window support

### Verification

1. Build the project: `cmake --build build`
2. Run with updated script: `./scripts/start_chess_match.sh`
3. Verify spectator window opens automatically after `configure_game`
4. Verify correct theme is applied