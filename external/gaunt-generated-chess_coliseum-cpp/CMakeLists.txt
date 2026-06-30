
# ---- Minimum Version ----
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----
project(gaunt-generated-chess_coliseum-cpp
        VERSION 0.0.1
        LANGUAGES CXX
)

option(GauntGenerated_ChessColiseum_UseExternalDependencyHelper
    "Use the auto-generated external dependency helper to find and build dependencies?"
    OFF
)

if (GauntGenerated_ChessColiseum_UseExternalDependencyHelper)
    include(cmake/gaunt-generated-chess_coliseum-handle-dependencies.cmake)
    GauntGenerated_ChessColiseum_HandleExternalDependencies()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(
        gaunt-core
        REQUIRED
        CONFIG
)

add_library(gaunt-generated-chess_coliseum-cpp
        INTERFACE
        #sources
        include/gaunt/generated/chess_coliseum.gaunt.hpp
)
add_library(gaunt::generated::chess_coliseum-cpp ALIAS gaunt-generated-chess_coliseum-cpp)

set_target_properties(
        gaunt-generated-chess_coliseum-cpp
        PROPERTIES
        PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/gaunt/generated/chess_coliseum.gaunt.hpp
        LINKER_LANGUAGE CXX
)

target_link_libraries(gaunt-generated-chess_coliseum-cpp
        INTERFACE
                gaunt::gaunt-core
)

target_include_directories(gaunt-generated-chess_coliseum-cpp
        INTERFACE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
        TARGETS gaunt-generated-chess_coliseum-cpp
        EXPORT gaunt-generated-chess_coliseum-cpp-targets
        LIBRARY
                DESTINATION lib
        ARCHIVE
                DESTINATION lib
        PUBLIC_HEADER
                DESTINATION include/gaunt/generated
        INCLUDES
                DESTINATION
                        include/gaunt/generated
)

install(
    EXPORT gaunt-generated-chess_coliseum-cpp-targets
    FILE gaunt-generated-chess_coliseum-cpp-targets.cmake
    NAMESPACE gaunt::generated::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gaunt/generated
)

write_basic_package_version_file(gaunt-generated-chess_coliseum-cpp-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion
)


configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/gaunt-generated-chess_coliseum-cpp-config.gaunt.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/gaunt-generated-chess_coliseum-cpp-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gaunt/generated
)


install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/gaunt-generated-chess_coliseum-cpp-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/gaunt-generated-chess_coliseum-cpp-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/gaunt/generated
)
