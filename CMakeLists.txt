cmake_minimum_required(VERSION 3.17)

project(Chess)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_custom_target(
    copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_CURRENT_LIST_DIR}
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SOURCES
    "src/Application/Main.cpp"
    "src/Application/Application.h"
    "src/Application/Application.cpp"
    "src/Application/ChessPanel.cpp"
    "src/Application/EnginePanel.cpp"
    "src/Application/SettingsPanel.cpp"
    src/Application/ConfigureGameWindow.h
    src/Application/ConfigureGameWindow.cpp
    src/Application/GameBrowserWindow.h
    src/Application/GameBrowserWindow.cpp
    "src/Application/AgentChat/AgentSidebar.h"
    "src/Application/AgentChat/AgentSidebar.cpp"
    "src/Application/AgentChat/AgentSidebarTrajectory.hpp"
    "src/Application/Resources.h"

    src/Application/CentralChessboardPanel.h
    src/Application/CentralChessboardPanel.cpp
    src/Application/ThreeColumnGameWindow.hpp
    src/Application/GameView.h
    src/Application/GameView.cpp
    src/Application/Command/Command.hpp
    src/Application/Command/CommandFile.hpp
    src/Application/Command/CommandFile.cpp
    src/Application/Command/ExternalCommandExecutor.hpp
    src/Game/GameContext.hpp
    src/Game/GameAudio.hpp
    src/Game/GameMoveLog.hpp
    src/Game/GameLifecyclePhase.hpp
    src/Game/Error/AgentMoveError.hpp

    src/Application/Task/BlockingTask.hpp
    src/Application/Task/TaskExecutor.hpp

    src/Application/Model/OpenRouterModelDetails.hpp
    src/Application/Model/AwsBedrockModelDetails.hpp
    src/Application/Model/ModelBrowserCache.hpp
    src/Application/Model/ModelBrowserColumns.hpp
    src/Application/Model/IModelBrowser.hpp
    src/Application/Model/OpenRouterModelBrowser.hpp
    src/Application/ModelBrowserWindow.h
    src/Application/ModelBrowserWindow.cpp

    src/Application/Provider/OpenRouterModelProviderConfiguration.hpp
    src/Application/Provider/AWSBedrockModelProviderConfiguration.hpp
    src/Application/Provider/ModelProviderConfiguration.hpp
    src/Application/ModelProviderConfigurationWindow.h
    src/Application/ModelProviderConfigurationWindow.cpp
    src/Application/ProvidersWindow.h
    src/Application/ProvidersWindow.cpp

    src/Application/AgentChat/ChatBubble/BaseBubble.h
    src/Application/AgentChat/ChatBubble/InfoBubble.h
    src/Application/AgentChat/ChatBubble/MoveBubble.h
    src/Application/AgentChat/ChatBubble/QuipBubble.h
    src/Application/AgentChat/ChatBubble/ReasoningBubble.h
    src/Application/AgentChat/ChatBubble/ResponseBubble.h

    src/Application/AgentChat/Message/BoardState.hpp
    src/Application/AgentChat/Message/GameFlow.hpp
    src/Application/AgentChat/Message/Handshake.hpp
    src/Application/AgentChat/Message/Utility.hpp

    "src/Chess/AlgebraicMove.cpp"
    "src/Chess/LongAlgebraicNotation.hpp"
    "src/Chess/LongAlgebraicNotation.cpp"
    "src/Chess/BitBoard.h"
    "src/Chess/Board.h"
    "src/Chess/Board.cpp"
    "src/Chess/BoardFormat.h"
    "src/Chess/ChessException.h"
    "src/Chess/Game.h"
    "src/Chess/Game.cpp"
    "src/Chess/PseudoLegal.h"
    "src/Chess/PseudoLegal.cpp"
    "src/Chess/Move.h"

    "src/Engine/Engine.h"
    "src/Engine/Engine.cpp"
    "src/Engine/EngineException.h"
    "src/Engine/Option.h"

#    "src/Graphics/Buffer.h"
#    "src/Graphics/Buffer.cpp"
#    "src/Graphics/DebugContext.h"
#    "src/Graphics/DebugContext.cpp"
#    "src/Graphics/Framebuffer.h"
#    "src/Graphics/Framebuffer.cpp"
#    "src/Graphics/Renderer.h"
#    "src/Graphics/Renderer.cpp"
#    "src/Graphics/Shader.h"
#    "src/Graphics/Shader.cpp"
#    "src/Graphics/SubTexture.h"
#    "src/Graphics/SubTexture.cpp"
#    "src/Graphics/Texture.h"
#    "src/Graphics/Texture.cpp"
#    "src/Graphics/VertexArray.h"
#    "src/Graphics/VertexArray.cpp"

    src/Game/Execution/GameOrchestrator.hpp
    src/Game/Execution/MatchResult.hpp
    src/Game/Execution/State.hpp
    src/Game/Execution/Message/Lifecycle.hpp
    src/Game/Execution/Message/PlayerAction.hpp
    src/Game/Execution/Message/MoveValidation.hpp
    src/Game/Execution/Message/Notification.hpp
    src/Game/View/InteractionState.hpp

    src/Graphics/RGBA.hpp
    src/Graphics/GLTexture.hpp
    src/Graphics/Board/BoardAtlas.hpp
    src/Graphics/Pieces/PieceAtlas.hpp
    src/Graphics/Pieces/PieceBorderAtlas.hpp
    src/Graphics/Icon/CheckDoubleSVG.hpp
    src/Graphics/Icon/WarningSVG.hpp
    src/Graphics/Sound/Confirm01.hpp
    src/Graphics/Sound/CollectPoint2.hpp
    src/Graphics/Sound/FireHit01.hpp
    src/Graphics/Sound/ActivatePower.hpp
    src/Graphics/Sound/NoEntry.hpp
    src/Graphics/SVG/Import.hpp

    src/Graphics/Theme/AgentSidebarColorPalette.hpp

    src/Graphics/Theme/Color/CreamBlueAgentSidebarColorPalette.hpp
    src/Graphics/Theme/Color/CreamAgentSidebarColorPalette.hpp
    src/Graphics/Theme/Color/DarkAgentSidebarColorPalette.hpp
    src/Graphics/Theme/Color/StandardAgentSidebarColorPalette.hpp
    src/Graphics/Theme/Color/WoodAgentSidebarColorPalette.hpp

    "src/Utility/FileDialog.h"
    "src/Utility/StringParser.h"
    "src/Utility/Timer.h"
    "src/Utility/ZMQAgentClient.cpp"
    "src/Utility/GameOrchestrator.cpp"
    "src/Utility/AgentTrajectory.h"
    "src/Utility/GameOrchestrator.h"
    "src/Utility/ZMQAgentClient.h"
    "src/Utility/ZMQPairChannel.hpp"
    "src/Utility/ZMQCommandServer.hpp"
    src/Utility/SDL3_STD.hpp

    src/Utility/Log/Level.hpp
    src/Utility/Log/MakeLog.hpp
    src/Utility/Log/Switch.hpp

    src/Telemetry/GauntTelemetry.hpp
    src/Telemetry/GauntTelemetry.cpp
    src/Telemetry/GameTelemetry.hpp
)

if (WIN32)
    set(SOURCES
        ${SOURCES}
        "src/Platform/Windows/WindowsEngine.h"
        "src/Platform/Windows/WindowsEngine.cpp"
        "src/Platform/Windows/WindowsFileDialog.cpp"
    )

    add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif (UNIX)
    set(SOURCES
        ${SOURCES}
        "src/Platform/Unix/UnixEngine.h"
        "src/Platform/Unix/UnixEngine.cpp"
        "src/Platform/Unix/UnixFileDialog.cpp"
    )

    add_executable(${PROJECT_NAME} ${SOURCES})
else()
    message(FATAL_ERROR "Unsupported platform!")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 26
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "src/"
    "${CMAKE_CURRENT_BINARY_DIR}/resources/"
)

target_compile_options(
    ${PROJECT_NAME}
    PRIVATE
    "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}/resources/"
    "--embed-dir=${CMAKE_CURRENT_LIST_DIR}/resources/fonts/pixel_operator/"
    "--embed-dir=${CMAKE_CURRENT_LIST_DIR}/resources/textures/"
    "--embed-dir=${CMAKE_CURRENT_LIST_DIR}/resources/sounds/"
)

# Structured trace logging. When enabled, the CHESS_*_TRACE / CHESS_TRACE_LOG
# macros (src/Utility/Log/Switch.hpp) compile in; otherwise they expand to
# nothing and cost zero at runtime. Toggle with -DCHESS_ENABLE_TRACE_LOGS=ON,
# or set CHESS_LOG_LEVEL to one of off/trace/debug/info/warn/error/critical for
# finer control over which levels are compiled in.
option(CHESS_ENABLE_TRACE_LOGS "Compile in TRACE-level structured logging" OFF)
set(CHESS_LOG_LEVEL "" CACHE STRING "Compiled-in log level: off|trace|debug|info|warn|error|critical")

if (CHESS_ENABLE_TRACE_LOGS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_TRACE=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "off")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_OFF=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "trace")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_TRACE=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_DEBUG=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "info")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_INFO=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "warn")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_WARN=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "error")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_ERROR=ON)
elseif (CHESS_LOG_LEVEL STREQUAL "critical")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHESS_INCLUDE_LOG_LEVEL_CRITICAL=ON)
endif()

###############
# Include tests
###############

add_subdirectory(tests)

################################
# Find and download dependencies
################################

include(FetchContent)

FetchContent_Declare(
    cool_vcpkg_latest
    GIT_REPOSITORY https://github.com/XJ-0461/cool-vcpkg.git
    GIT_TAG "v0.1.3"
    SOURCE_SUBDIR automatic-setup
)
FetchContent_MakeAvailable(cool_vcpkg_latest)

include(CoolVcpkg)

set(cool_vcpkg_triplet_ x64-linux)
if (WIN32)
    set(cool_vcpkg_triplet_ x64-windows)
endif()

cool_vcpkg_SetUpVcpkg(
    DEFAULT_TRIPLET ${cool_vcpkg_triplet_}
    ROOT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/external/cool-vcpkg/"
    AUTO_UPDATE_BASELINE ON
)

cool_vcpkg_DeclarePackage(NAME nlohmann-json)
cool_vcpkg_DeclarePackage(NAME cppzmq)
cool_vcpkg_DeclarePackage(NAME concurrentqueue)
cool_vcpkg_DeclarePackage(NAME sdl3 VERSION "3.4.10#1" FEATURES wayland x11 vulkan alsa)
cool_vcpkg_DeclarePackage(NAME sdl3-image VERSION "3.4.4#0" FEATURES png webp jpeg)
cool_vcpkg_DeclarePackage(NAME sdl3-mixer VERSION "3.2.4#0" FEATURES wavpack)
cool_vcpkg_DeclarePackage(NAME imgui VERSION "1.92.8#0"
    FEATURES docking-experimental sdl3-binding sdl3-renderer-binding sdlgpu3-binding opengl3-binding
)
cool_vcpkg_DeclarePackage(NAME sobjectizer VERSION "5.8.5.1#0")
cool_vcpkg_DeclarePackage(NAME cpr VERSION "1.14.2#0")
cool_vcpkg_DeclarePackage(NAME spdlog)
cool_vcpkg_DeclarePackage(NAME xerces-c)
cool_vcpkg_DeclarePackage(NAME cnats)
cool_vcpkg_DeclarePackage(NAME tiny-process-library)
cool_vcpkg_DeclarePackage(NAME gtest)
cool_vcpkg_DeclarePackage(NAME openssl)
cool_vcpkg_DeclarePackage(NAME tbb)

cool_vcpkg_InstallPackages()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_mixer CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SObjectizer CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
#find_package(XercesC REQUIRED)
#find_package(OpenSSL REQUIRED)
#find_package(cnats CONFIG REQUIRED)
find_package(tiny-process-library CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# --- Gaunt Telemetry ---
include(cmake/handle-dependencies.cmake)
HandleDependency_NatsCxxWrapper(GIT_TAG master)
HandleDependency_GauntCore(GIT_TAG 31eb7d6357dbea4bc63e572d0f16ecc43949bf7d)

add_subdirectory(external/gaunt-generated-chess_coliseum-cpp)

function(Chess_GenerateRGBA)
    set(oneValueArgs PNG_PATH OUTPUT_PATH)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_PNG_PATH)
        message(FATAL_ERROR "Chess_GenerateAtlas: PNG_PATH is required")
    endif()

    if(NOT ARG_OUTPUT_PATH)
        message(FATAL_ERROR "Chess_GenerateAtlas: OUTPUT_PATH is required")
    endif()

    find_program(_imagemagick_exe NAMES magick convert)

    if(NOT _imagemagick_exe)
        message(FATAL_ERROR "ImageMagick ('magick' or 'convert') not found. RGBA atlas generation cannot be performed.")
    endif()

    get_filename_component(OUTPUT_DIR "${ARG_OUTPUT_PATH}" DIRECTORY)

    add_custom_command(
        OUTPUT "${ARG_OUTPUT_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
        COMMAND "${_imagemagick_exe}" "${ARG_PNG_PATH}" -depth 8 "rgba:${ARG_OUTPUT_PATH}"
        DEPENDS "${ARG_PNG_PATH}"
        COMMENT "Generating RGBA atlas: ${ARG_OUTPUT_PATH}"
        VERBATIM
    )

    target_sources(${PROJECT_NAME} PRIVATE "${ARG_OUTPUT_PATH}")
endfunction()

Chess_GenerateRGBA(
    PNG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/textures/Pieces.png"
    OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/resources/Pieces.rgba"
)

Chess_GenerateRGBA(
    PNG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/textures/Board.png"
    OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/resources/Board.rgba"
)

# The .wav sound assets are intentionally not tracked in git. When they are
# missing (e.g. a fresh clone), generate a silent, zero-length WAV placeholder
# in their place so the #embed directives in src/Graphics/Sound/ still resolve
# and the project builds. Real assets, once present, are left untouched.
function(Chess_CreatePlaceholderWavResourceIfNotExists)
    set(oneValueArgs PATH)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_PATH)
        message(FATAL_ERROR "Chess_CreatePlaceholderWavResourceIfNotExists: PATH is required")
    endif()

    if(EXISTS "${ARG_PATH}")
        return()
    endif()

    find_program(_ffmpeg_exe NAMES ffmpeg)

    if(NOT _ffmpeg_exe)
        message(FATAL_ERROR "ffmpeg not found; cannot generate placeholder WAV '${ARG_PATH}'. Install ffmpeg or supply the real sound asset.")
    endif()

    get_filename_component(_placeholder_dir "${ARG_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${_placeholder_dir}")

    message(STATUS "${ARG_PATH} not found - generating placeholder .wav file")
    execute_process(
        COMMAND "${_ffmpeg_exe}"
            -hide_banner -loglevel error
            -f lavfi -i anullsrc=r=44100:cl=mono -t 0
            -c:a pcm_s16le -y "${ARG_PATH}"
        RESULT_VARIABLE _ffmpeg_result
        ERROR_VARIABLE _ffmpeg_error
    )

    if(NOT _ffmpeg_result EQUAL 0)
        message(FATAL_ERROR "ffmpeg failed to generate placeholder WAV '${ARG_PATH}':\n${_ffmpeg_error}")
    endif()

    message(STATUS "Created placeholder WAV resource: ${ARG_PATH}")
endfunction()

set(_chess_wav_resource_files
    "Activate_Power.wav"
    "Collect_Point_2.wav"
    "Confirm_01.wav"
    "Fire_Hit_01.wav"
    "No_Entry.wav"
)

foreach(_wav_file IN LISTS _chess_wav_resource_files)
    Chess_CreatePlaceholderWavResourceIfNotExists(
        PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds/${_wav_file}"
    )
endforeach()

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    imgui::imgui
    OpenGL::GL
    nlohmann_json::nlohmann_json
    cppzmq-static
    concurrentqueue::concurrentqueue
    SDL3::SDL3
    SDL3_image::SDL3_image-static
    SDL3_mixer::SDL3_mixer-static
    sobjectizer::StaticLib
    cpr::cpr
    spdlog::spdlog
    gaunt::generated::chess_coliseum-cpp
    TBB::tbb
)

