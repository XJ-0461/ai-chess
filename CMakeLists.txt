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
    "src/Application/Resources.h"

    src/Application/CentralChessboardPanel.h
    src/Application/CentralChessboardPanel.cpp
    src/Application/ThreeColumnGameWindow.hpp

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

    src/Graphics/RGBA.hpp
    src/Graphics/Board/BoardAtlas.hpp
    src/Graphics/Pieces/PieceAtlas.hpp
    src/Graphics/Icon/CheckDoubleSVG.hpp
    src/Graphics/Icon/WarningSVG.hpp
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
    "src/Utility/ZMQAgentServer.cpp"
    "src/Utility/GameOrchestrator.cpp"
    "src/Utility/AgentTrajectory.h"
    "src/Utility/GameOrchestrator.h"
    "src/Utility/ZMQAgentServer.h"
    "src/Utility/ZMQPairChannel.hpp"
    src/Utility/SDL3_STD.hpp
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

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

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
)

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
cool_vcpkg_DeclarePackage(NAME sdl3 VERSION "3.4.10#1" FEATURES wayland x11 vulkan)
cool_vcpkg_DeclarePackage(NAME sdl3-image VERSION "3.4.4#0" FEATURES png webp jpeg)
cool_vcpkg_DeclarePackage(NAME sdl3-mixer VERSION "3.2.4#0" FEATURES wavpack)
cool_vcpkg_DeclarePackage(NAME imgui VERSION "1.92.8#0"
    FEATURES docking-experimental sdl3-binding sdl3-renderer-binding sdlgpu3-binding opengl3-binding
)
cool_vcpkg_DeclarePackage(NAME sobjectizer VERSION "5.8.5.1#0")
cool_vcpkg_DeclarePackage(NAME cpr VERSION "1.14.2#0")

cool_vcpkg_InstallPackages()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SObjectizer CONFIG REQUIRED)

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

# GLFW
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    imgui::imgui
    ${OPENGL_LIBRARY}
    nlohmann_json::nlohmann_json
    cppzmq-static
    concurrentqueue::concurrentqueue
    SDL3::SDL3
    SDL3_image::SDL3_image-static
    sobjectizer::StaticLib
)

