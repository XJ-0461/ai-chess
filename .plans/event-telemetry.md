# Event Telemetry

We will make use of Gaunt Telemetry to log the rich event details that happen within the chess games.

It is available in: /home/tristan/Dev/chess-ai/ai-chess/external/gaunt-generated-chess_coliseum-cpp

The overall outline of the Gaunt Telemetry Context is like this:

./Root
└── ./Root/Session
    └── ./Root/Session/Game
        └── ./Root/Session/Game/Player
            └── ./Root/Session/Game/Player/Move
## Flow

### On Application Init

Setup the Root context.
Setup sinks for telemetry to be routed to.

Setup the Root/Session(UUID) context. 

### On Game Configuration

Setup the Root/Session/Game context (from the Root context handler).

### On Player Configuration

When the players are actually configured, then we can initialize the Root/Session/Game/Player context

### On Move Begin

Setup the Root/Session/Game/Player/Move context to keep track of events that occur within the Move context.

### On Move End

End the context by sending in the appropriate Event Type.

### On Game End

End the contexts dependent on Game context first (Player context).
End the game by sending in the appropriate Event Type.

### On Application Close

Before application closes, end the Session context, then end the Root context. Flush the telemetry to the sink.

## Implementation Details

All context handles should be created via the parent context's custom constructor.
Application.h should maintain a struct to keep context handles alive for the duration of the application.

```cpp
struct GauntContextHandles {
    std::shared_ptr<Root> root_context;
    std::shared_ptr<Session> session_context;
} 
```

Expose some simple GauntContextHandles& GetGauntContextHandles() method from the Application that we can use through the singleton for easy access.

We will use sinks:
    - XML Output Sink
    - Console logging output sink (driven by spdlog similar to the other logging in the program)

Accept a program argument for an --gaunt-telemetry-xml-output-file <PATH>, if this is not set, then dont use the xml sink.

The Session context is keyed by a string, in our case, we will always use a random UUID as the key.
The Game context should be keyed by a monotonically increasing integer counter (to_string()).

The GameOrchestrator will maintain handles to the Game context and each of the Player contexts, so that events can be added to the telemetry from these handles.

std::shared_ptr<Game> game_context;

struct GauntPlayerContext {
std::shared_ptr<Player> player_context;
std::shared_ptr<Move> current_move_context;
}

then we can instantiate two of these GauntPlayerContext, one for white, one for black.

Then, for example, when the player makes a move (during the action phase), the orchestrator can also send the event via the handler.

## Gaunt Dependency

The gaunt-generated-chess_coliseum-cpp library depends on the gaunt library, we should fetch this from github in a similar style to this.

```cmake
macro(BuildExternalDependencyFromGit_Dreamlands)

    set(oneValueArgs GIT_TAG GIT_REPO)
    cmake_parse_arguments(beg_dreamlands "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED beg_dreamlands_GIT_REPO OR beg_dreamlands_GIT_REPO STREQUAL "")
        set(beg_dreamlands_GIT_REPO "git@github.com:XJ-0461/dreamlands-cpp.git")
    endif()

    if (DEFINED Dreamlands_DIR
        AND NOT Dreamlands_DIR STREQUAL ""
        AND NOT Dreamlands_DIR STREQUAL "Dreamlands_DIR-NOTFOUND"
    )
        message(STATUS "[ThrashService] Dreamlands SDK already configured (Dreamlands_DIR=${Dreamlands_DIR}), skipping Git fetch.")
    else()
        if (NOT DEFINED beg_dreamlands_GIT_TAG OR beg_dreamlands_GIT_TAG STREQUAL "")
            message(FATAL_ERROR "[ThrashService] BuildExternalDependencyFromGit_Dreamlands: GIT_TAG must be set.")
        endif()

        message(STATUS "[ThrashService] Fetching Dreamlands SDK from ${beg_dreamlands_GIT_REPO} @ ${beg_dreamlands_GIT_TAG}")

        include(FetchContent)
        FetchContent_Populate(
            thrash_dreamlands
            GIT_REPOSITORY "${beg_dreamlands_GIT_REPO}"
            GIT_TAG        "${beg_dreamlands_GIT_TAG}"
            GIT_SHALLOW    TRUE
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/thrash_dreamlands-src"
            SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/thrash_dreamlands-subbuild"
            BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/thrash_dreamlands-build"
        )

        set(_dreamlands_install_dir "${thrash_dreamlands_BINARY_DIR}/_install")

        set(_dreamlands_toolchain_arg "")
        if (CMAKE_TOOLCHAIN_FILE)
            set(_dreamlands_toolchain_arg "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
        endif()

        # Dreamlands needs to run cool-vcpkg itself to set up CMAKE_PREFIX_PATH for its dependencies
        # (Crow, cpr, jwt-cpp, nlohmann_json). By sharing Auto_RootDirectory with the main
        # project, cool-vcpkg finds those packages already installed and skips rebuilding them.
        message(STATUS "[ThrashService] Configuring Dreamlands SDK...")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                -S "${thrash_dreamlands_SOURCE_DIR}"
                -B "${thrash_dreamlands_BINARY_DIR}"
                -G "${CMAKE_GENERATOR}"
                -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${_dreamlands_install_dir}
                ${_dreamlands_toolchain_arg}
                "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
                -DDreamlands_ExternalDependency_Auto_DefaultTriplet=${ThrashService_ExternalDependency_Auto_DefaultTriplet}
                -DDreamlands_ExternalDependency_Auto_GitTag=${ThrashService_ExternalDependency_Auto_GitTag}
                -DDreamlands_ExternalDependency_Auto_RootDirectory=${ThrashService_ExternalDependency_Auto_RootDirectory}
                -DDreamlands_ExternalDependency_Botan_Auto_Fetch=OFF
                -DDreamlands_ExternalDependency_Cpr_Auto_Fetch=OFF
                -DDreamlands_ExternalDependency_JwtCpp_Auto_Fetch=OFF
                -DDreamlands_ExternalDependency_NlohmannJson_Auto_Fetch=OFF
            RESULT_VARIABLE _dreamlands_configure_result
        )
        if (_dreamlands_configure_result)
            message(FATAL_ERROR "[ThrashService] Dreamlands configure step failed (exit: ${_dreamlands_configure_result}).")
        endif()

        message(STATUS "[ThrashService] Building Dreamlands SDK...")
        execute_process(
                COMMAND ${CMAKE_COMMAND} --build "${thrash_dreamlands_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
                RESULT_VARIABLE _dreamlands_build_result
        )
        if (_dreamlands_build_result)
            message(FATAL_ERROR "[ThrashService] Dreamlands SDK build step failed (exit: ${_dreamlands_build_result}).")
        endif()

        message(STATUS "[ThrashService] Installing Dreamlands SDK...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --install "${thrash_dreamlands_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
            RESULT_VARIABLE _dreamlands_install_result
        )
        if (_dreamlands_install_result)
            message(FATAL_ERROR "[ThrashService] Dreamlands SDK install step failed (exit: ${_dreamlands_install_result}).")
        endif()

        list(PREPEND CMAKE_PREFIX_PATH "${_dreamlands_install_dir}")
        find_package(Dreamlands CONFIG REQUIRED)
        message(STATUS "[ThrashService] Dreamlands SDK installed to ${_dreamlands_install_dir}.")
    endif()

endmacro()

BuildExternalDependencyFromGit_Dreamlands(GIT_REPO git@github.com:XJ-0461/gaunt-core.git GIT_TAG master)

```

## Simple Example from other implementation

These are usage examples from another implementation, the coding style and practices should be bespoke to the Chess application though. This is just a usage reference.

```c++
auto setup_gaunt_context(ContextsHolder& contexts, const Args& args) {

    // print working directory
    std::cout << std::format("Current working directory: {}\n", std::filesystem::current_path().string());

    const std::filesystem::path output_dir = "/output/gaunt_stream";
    std::filesystem::create_directories(output_dir);
    const std::filesystem::path output_file_path =
        output_dir / (args.gaunt_stream_id + ".gaunt.stream.xml");
    gaunt::core::XMLSinkOptions xml_sink_options{
        .output_file_path = output_file_path,
        .create_file_if_not_exists = true
    };
    auto xml_sink_ptr = std::make_shared<gaunt::core::XMLSink>(xml_sink_options);

    auto custom_sink_ptr = gaunt::core::MakeCustomSharedSink<gaunt::core::XMLDebugFormatResult>(
        [](std::monostate, const std::any&, const gaunt::core::XMLDebugFormatResult& formatted, const std::any&) -> void {
            std::cout << std::format("[custom sink] Received event: {}\n", formatted.as_string);
        }
    );
    using CustomSinkType = decltype(custom_sink_ptr);

    auto shared_sink_manager = gaunt::core::SharedSinkManagerBuilder<ggpg::TypeErasedEventConverter>{}
        .WithFormatter<gaunt::core::XMLDebugFormatter>()
        .WithSink<std::shared_ptr<gaunt::core::XMLSink>>()
        .WithSink<CustomSinkType>()
        .BuildShared();
    shared_sink_manager->RegisterFormatter(gaunt::core::XMLDebugFormatter{});
    shared_sink_manager->RegisterSink(xml_sink_ptr);
    shared_sink_manager->RegisterSink(custom_sink_ptr);

    gaunt::core::GauntContext<ggpg::context::Root> root_context{ggpg::context::Root{}, gaunt::core::ContextPath("Root")};
    root_context.AttachSinkManager(shared_sink_manager);
    root_context.Accept(ggpg::event::RootBegin{});

    auto session_ctx = root_context.CreateChildContext<ggpg::context::Session>(
        ggpg::context::Session::Key{"0"},
        ggpg::event::SessionBegin{}
    );
    auto race_ctx = session_ctx.CreateChildContext<ggpg::context::Race>(
        ggpg::context::Race::Key{"0"},
        ggpg::event::RaceBegin{}
    );
    contexts.root_context = std::move(root_context);
    contexts.session_context = std::move(session_ctx);
    contexts.race_context = std::move(race_ctx);

    return shared_sink_manager;
}
```

```cpp
void close_all_gaunt_contexts(ContextsHolder& contexts) {

    for (auto& player_ctx : contexts.player_contexts | std::views::values) {
        player_ctx.Accept(ggpg::event::PlayerEnd{});
    }

    contexts.race_context.Accept(ggpg::event::RaceEnd{});
    contexts.session_context.Accept(ggpg::event::SessionEnd{});
    contexts.root_context.Accept(ggpg::event::RootEnd{});

}
```

