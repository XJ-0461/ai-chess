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
    "src/Application/Resources.h"

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

    "src/Graphics/Buffer.h"
    "src/Graphics/Buffer.cpp"
    "src/Graphics/DebugContext.h"
    "src/Graphics/DebugContext.cpp"
    "src/Graphics/Framebuffer.h"
    "src/Graphics/Framebuffer.cpp"
    "src/Graphics/Renderer.h"
    "src/Graphics/Renderer.cpp"
    "src/Graphics/Shader.h"
    "src/Graphics/Shader.cpp"
    "src/Graphics/SubTexture.h"
    "src/Graphics/SubTexture.cpp"
    "src/Graphics/Texture.h"
    "src/Graphics/Texture.cpp"
    "src/Graphics/VertexArray.h"
    "src/Graphics/VertexArray.cpp"

    "src/Utility/FileDialog.h"
    "src/Utility/StringParser.h"
    "src/Utility/Timer.h"
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
    CXX_STANDARD 17
)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "src/"
)

###############
# Include tests
###############

add_subdirectory(tests)

################################
# Find and download dependencies
################################

include(FetchContent)

# set(IMGUI_VERSION "1.92.6")
set(GLM_VERSION "1.0.3")
set(GLFW_VERSION "3.4")

# GLFW
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)

FetchContent_Declare(
    imgui
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    # URL https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.tar.gz
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG 934c6a5f5ef2355d6df25395d555cb71f790c4e9
)

FetchContent_Declare(
    glm
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/g-truc/glm/releases/download/${GLM_VERSION}/glm-${GLM_VERSION}.7z
)

FetchContent_Declare(
    glfw
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz
)

FetchContent_MakeAvailable(imgui glm glfw)

set(IMGUI_SOURCES
    "${imgui_SOURCE_DIR}/imconfig.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui_internal.h"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imstb_rectpack.h"
    "${imgui_SOURCE_DIR}/imstb_textedit.h"
    "${imgui_SOURCE_DIR}/imstb_truetype.h"

    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui ${IMGUI_SOURCES})

target_include_directories(
    imgui
    PUBLIC
    ${imgui_SOURCE_DIR}
)

target_link_libraries(imgui PRIVATE glfw)
target_compile_definitions(imgui PRIVATE GLFW_INCLUDE_NONE)

#############
# Find OpenGL
#############

find_package(OpenGL REQUIRED)
if (NOT OpenGL_FOUND)
    message(FATAL_ERROR "OpenGL not found")
endif()
message("OPENGL_FOUND:${OpenGL_FOUND}")
message("OPENGL_LIBRARIES:${OpenGL_LIBRARIES}")

# glad
add_subdirectory(lib/glad)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    imgui
    glad
    ${OPENGL_LIBRARY}
)

set_target_properties(glfw update_mappings glad imgui PROPERTIES FOLDER "DEPENDENCIES/")

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "lib/stb_image/"
    "${glm_SOURCE_DIR}/"
    "${imgui_SOURCE_DIR}/imgui/"
)
