
## Non-functional requirements

#### Style

- Always use { ... } for if/then/else
```c++
if (something) {
    ...
}
```

- use 'const' variables consistently and whenever possible
- constexpr/global constants in PascalCase prefixed with 'k' e.g. kSomethingLikeThis
- local variables in lower_snake_case
- Private member variables lower snake case with trailing '_' e.g. something_like_this_
- Use std::unique/std::shared_ptr (with custom deleter as needed) over raw pointers wherever possible
- Must use Dear ImGui modern practices and best practices
- Must use c++23 best practices
- Use constexpr wherever possible
- Use sobjectizer best practices
  - We can reach for so5extra library if necessary, but we should avoid unless totally necessary
- Use appropriate namespacing

## Goals

- Cross platform rendering that supports multiple windows
  - I should be able to isolate these windows in a gpu-screen-recorder (cli or gui tool) context so that I can record only the windows im interested in
  - This means I need to migrate back to opengl backend for imgui because the SDL3 backend doesn't support multiple windows (multi viewport feature)
  - Need a plan for migrating the SDL features to opengl3 equivalents and to keep them as nice and comfortable as the SDL3 counterparts
    - 
- ModelProviderConfigurationWindow.hpp
  - Drop down to select the 'kind' of provider to setup (options: OpenRouter or AWS Bedrock)
    - Present the appropriate form rendering depending on this selection
- OpenRouterModelProviderConfiguration.hpp
  - api_key_
- AWSBedrockModelProviderConfiguration.hpp
  - Aws Bedrock login settings
    - access_key + secret_key
- Model browser to make it easier to make games between the models
  - IModelBrowser.hpp
    - FetchModels()
      - save the results to the application ModelBrowserCache (just use a std::vector<variant<OpenRouterModelDetails, AwsBedrockModelDetails>> for now - will key each by std::string{ (OpenRouter|AWSBedrock):model_id })
      - implementations can make use of the shared "task executor" sobjectizer 'BlockingTask' job pool for long-running requests.
  - OpenRouterModelBrowser.hpp
  - AWSBedrockModelBrowser.hpp
- OpenRouterModelDetails.hpp
  - Details cost information (especially pricing information on cost per input and output token)
  - Details throughput information
- AwsBedrockModelDetails.hpp
  - Details cost information
    - Some models have different tiers based on your subscription information, most critical is the on-demand pricing
- ModelBrowserWindow.hpp
  - window to view items in the cache and sort based on price low->high or high->low and throughput
- Handle configuring games via ConfigureGameWindow.hpp
  - 'Create' button should initialize the GameOrchestrator in the application's sobjectizer wrapped_env context 
- Handle starting/viewing games via GameBrowserWindow.hpp
  - Buttons for 'Open Spectator View' and 'Open Spectator View (high contrast)'
    - 'Open Spectator View' should spawn a ThreeColumnGameWindow with the game context injected
    - 'Open Spectator View (high contrast)' should spawn a ThreeColumnGameWindow with game context injected and the default color scheme/theme
  - Button to "Start" game
    - sends a signal to that particular GameOrchestrator's mbox to start the game
- Separate the "Game" state from the view state/interaction state
  - I want a "Game" to supply the source of truth for many different views that can be spawned concurrently
  - e.g. I want the ThreeColumnGameWindow to be one of many different views that can be spawned to view a game
- Refactor most structs and definitions into their appropriate domain-driven-design file location
  - Currently most things are stuffed into a single file e.g. GameOrchestrator.hpp
- Ensure we are using namespaces consistently and with a domain-driven design

## Prompt

You are an Expert C++23 Software Engineer and Domain-Driven Design (DDD) Architect. Your task is to incrementally refactor and build out a complex AI-Orchestrated Chess Application. 

### Context
This application has transitioned from a simple chess player to an advanced orchestrator for AI agents playing against each other. It heavily relies on **Dear ImGui** for the UI, **Sobjectizer** for actor-model concurrency, and modern C++23 features. 

### Core Directives & Standards
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
   - Adhere strictly to **Domain-Driven Design (DDD)** principles. Refactor bloated files (like `GameOrchestrator.hpp`) into appropriate domain-specific structures.
   - Employ **Sobjectizer** best practices. Use the actor model effectively; utilize `BlockingTask` for long-running jobs (e.g., API requests). Avoid `so5extra` unless strictly necessary.
   - Decouple the "Game" state from the view/interaction state so that a single Game source of truth can drive multiple concurrent views.

3. **UI/Rendering:**
   - Use modern **Dear ImGui** practices.
   - The application must support **cross-platform rendering with multiple viewports** (windows) to allow selective window recording via tools like `gpu-screen-recorder`. 
   - Migrate from the SDL3 backend to the **OpenGL3 backend** to fully support ImGui's multi-viewport feature, while retaining the quality-of-life features of SDL3.

### Iterative Implementation Plan
You will be working on multiple long-running tasks. We will tackle them iteratively. Do not try to implement everything at once. Focus on the requested step while keeping the grand architecture in mind. 

The primary roadmap includes:
1. **Backend Migration:** Migrate ImGui rendering from SDL3 to OpenGL3 to support multi-viewports.
2. **Domain-Driven Refactoring:** Restructure existing entities (like `GameOrchestrator`) into their DDD domains. Ensure the "Game" state is separated from view representations.
3. **Model Provider Configuration:** Implement `ModelProviderConfigurationWindow` supporting OpenRouter (`api_key_`) and AWS Bedrock (`access_key`, `secret_key`).
4. **Model Browsing System:** Build `IModelBrowser` and specific implementations (OpenRouter, AWS) that fetch models asynchronously via Sobjectizer `BlockingTask`s, caching results, and visualizing them in `ModelBrowserWindow` sorted by price/throughput.
5. **Game Orchestration & Viewing:** Update `ConfigureGameWindow` to dispatch game creation signals. Build `GameBrowserWindow` to manage running games and spawn multiple detached spectator views (`ThreeColumnGameWindow`), ensuring proper state synchronization via Sobjectizer.

### Interaction Instructions
When asked to implement a feature:
- Start by analyzing the current codebase and outlining the DDD file structure you intend to create or modify.
- Provide the code changes using surgical precision.
- Confirm that your implementation strictly adheres to the styling and architectural constraints before finalizing the code.