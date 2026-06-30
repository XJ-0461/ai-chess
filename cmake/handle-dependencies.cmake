# Chess Coliseum External Dependency Handler
# This module provides macros for fetching external dependencies via CMake's FetchContent

# Fetch nats-cxx-wrapper library from Git
macro(HandleDependency_NatsCxxWrapper)
    set(oneValueArgs GIT_TAG GIT_REPO)
    cmake_parse_arguments(nats_wrapper_args "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED nats_wrapper_args_GIT_REPO OR nats_wrapper_args_GIT_REPO STREQUAL "")
        set(nats_wrapper_args_GIT_REPO "git@github.com:XJ-0461/nats-cxx-wrapper.git")
    endif()

    if (NOT DEFINED nats_wrapper_args_GIT_TAG OR nats_wrapper_args_GIT_TAG STREQUAL "")
        set(nats_wrapper_args_GIT_TAG "master")
    endif()

    message(STATUS "[Chess] Fetching nats-cxx-wrapper from ${nats_wrapper_args_GIT_REPO} @ ${nats_wrapper_args_GIT_TAG}")

    include(FetchContent)
    FetchContent_Populate(
        nats_cxx_wrapper
        GIT_REPOSITORY "${nats_wrapper_args_GIT_REPO}"
        GIT_TAG        "${nats_wrapper_args_GIT_TAG}"
        GIT_SHALLOW    TRUE
        SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nats_cxx_wrapper-src"
        SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nats_cxx_wrapper-subbuild"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nats_cxx_wrapper-build"
    )

    set(_nats_wrapper_install_dir "${nats_cxx_wrapper_BINARY_DIR}/_install")

    set(_nats_wrapper_toolchain_arg "")
    if (CMAKE_TOOLCHAIN_FILE)
        set(_nats_wrapper_toolchain_arg "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    elseif (_cool_vcpkg_toolchain_file)
        set(_nats_wrapper_toolchain_arg "-DCMAKE_TOOLCHAIN_FILE=${_cool_vcpkg_toolchain_file}")
    endif()

    message(STATUS "[ChessAI] Configuring nats-cxx-wrapper Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
        -S "${nats_cxx_wrapper_SOURCE_DIR}"
        -B "${nats_cxx_wrapper_BINARY_DIR}"
        -G "${CMAKE_GENERATOR}"
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${_nats_wrapper_install_dir}
        ${_nats_wrapper_toolchain_arg}
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        -DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}
        -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}
        RESULT_VARIABLE _nats_wrapper_configure_result
    )
    if (_nats_wrapper_configure_result)
        message(FATAL_ERROR "[ChessAI] nats-cxx-wrapper configure step failed (exit: ${_nats_wrapper_configure_result}).")
    endif()

    message(STATUS "[ChessAI] Building nats-cxx-wrapper Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${nats_cxx_wrapper_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
        RESULT_VARIABLE _nats_wrapper_build_result
    )
    if (_nats_wrapper_build_result)
        message(FATAL_ERROR "[ChessAI] nats-cxx-wrapper library build step failed (exit: ${_nats_wrapper_build_result}).")
    endif()

    message(STATUS "[ChessAI] Installing nats-cxx-wrapper Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install "${nats_cxx_wrapper_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
        RESULT_VARIABLE _nats_wrapper_install_result
    )
    if (_nats_wrapper_install_result)
        message(FATAL_ERROR "[ChessAI] nats-cxx-wrapper library install step failed (exit: ${_nats_wrapper_install_result}).")
    endif()

    list(PREPEND CMAKE_PREFIX_PATH "${_nats_wrapper_install_dir}")
    find_package(nats-cxx-wrapper CONFIG REQUIRED)
    message(STATUS "[ChessAI] nats-cxx-wrapper Library installed to ${_nats_wrapper_install_dir}.")

endmacro()

# Fetch gaunt-core library from Git
macro(HandleDependency_GauntCore)
    set(oneValueArgs GIT_TAG GIT_REPO)
    cmake_parse_arguments(gaunt_args "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED gaunt_args_GIT_REPO OR gaunt_args_GIT_REPO STREQUAL "")
        set(gaunt_args_GIT_REPO "git@github.com:XJ-0461/gaunt-core.git")
    endif()

    if (NOT DEFINED gaunt_args_GIT_TAG OR gaunt_args_GIT_TAG STREQUAL "")
        set(gaunt_args_GIT_TAG "master")
    endif()

    message(STATUS "[Chess] Fetching gaunt-core from ${gaunt_args_GIT_REPO} @ ${gaunt_args_GIT_TAG}")

    include(FetchContent)
    # Skip the git update/checkout step on reconfigures once the source is populated,
    # so editing CMake code doesn't re-fetch gaunt-core every time.
    set(FETCHCONTENT_UPDATES_DISCONNECTED_GAUNT_CORE ON)
    FetchContent_Populate(
        gaunt_core
        GIT_REPOSITORY "${gaunt_args_GIT_REPO}"
        GIT_TAG        "${gaunt_args_GIT_TAG}"
        GIT_SHALLOW    TRUE
        SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/gaunt_core-src"
        SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/gaunt_core-subbuild"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/gaunt_core-build"
    )

    set(_gaunt_core_install_dir "${gaunt_core_BINARY_DIR}/_install")

    set(_gaunt_core_toolchain_arg "")
    if (CMAKE_TOOLCHAIN_FILE)
        set(_gaunt_core_toolchain_arg "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    elseif (_cool_vcpkg_toolchain_file)
        set(_gaunt_core_toolchain_arg "-DCMAKE_TOOLCHAIN_FILE=${_cool_vcpkg_toolchain_file}")
    endif()

    message(STATUS "[ChessAI] Configuring Gaunt Core Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
        -S "${gaunt_core_SOURCE_DIR}"
        -B "${gaunt_core_BINARY_DIR}"
        -G "${CMAKE_GENERATOR}"
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${_gaunt_core_install_dir}
        ${_gaunt_core_toolchain_arg}
        "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        -DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}
        -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}
        # Disable all auto-fetch flags - we provide dependencies via vcpkg or HandleDependency_*
        -DGauntCore_ExternalDependency_XercesC_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_cnats_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_TinyProcessLibrary_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_NatsCxxWrapper_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_spdlog_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_gtest_Auto_Fetch=OFF
        -DGauntCore_ExternalDependency_concurrentqueue_Auto_Fetch=OFF
        -DGauntCore_NatsCxxWrapper_Fetch=OFF
        RESULT_VARIABLE _gaunt_core_configure_result
    )
    if (_gaunt_core_configure_result)
        message(FATAL_ERROR "[ChessAI] Gaunt-Core configure step failed (exit: ${_gaunt_core_configure_result}).")
    endif()

    message(STATUS "[ChessAI] Building Gaunt Core Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${gaunt_core_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
        RESULT_VARIABLE _gaunt_core_build_result
    )
    if (_gaunt_core_build_result)
        message(FATAL_ERROR "[ChessAI] Gaunt-Core library build step failed (exit: ${_gaunt_core_build_result}).")
    endif()

    message(STATUS "[ChessAI] Installing Gaunt Core Library...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install "${gaunt_core_BINARY_DIR}" --config ${CMAKE_BUILD_TYPE}
        RESULT_VARIABLE _gaunt_core_install_result
    )
    if (_gaunt_core_install_result)
        message(FATAL_ERROR "[ChessAI] Gaunt-Core library install step failed (exit: ${_gaunt_core_install_result}).")
    endif()

    list(PREPEND CMAKE_PREFIX_PATH "${_gaunt_core_install_dir}")
    # gaunt-core installs its package config under a nested cmake/gaunt/core/ path,
    # which find_package(gaunt-core) does not search (it only looks in dirs matching
    # the package-name glob, e.g. cmake/gaunt-core/). Point it at the real location.
    file(GLOB _gaunt_core_config_dir "${_gaunt_core_install_dir}/lib*/cmake/gaunt/core")
    if (_gaunt_core_config_dir)
        set(gaunt-core_DIR "${_gaunt_core_config_dir}")
    endif()
    find_package(gaunt-core CONFIG REQUIRED)
    message(STATUS "[ChessAI] Gaunt Core Library installed to ${_gaunt_core_install_dir}.")

endmacro()