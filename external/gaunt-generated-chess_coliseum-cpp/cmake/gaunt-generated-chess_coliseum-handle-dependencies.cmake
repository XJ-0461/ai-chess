

# List of variables that can be set to control the behavior of this script.
# I tried to help users fall into the pit of success. The CMakePresets supplied with this project should make it trivial
# to build. Users are strongly recommended to use a preset or otherwise automatically fetch dependencies, which is
# driven by vcpkg and is very reliable.

# However, sometimes there are reasons to manually specify which dependencies to use.
# For example: This project links against a Lua library "liblua.*" but if the project you need to integrate this with is
# using a custom lua library named "liblua53.*", then we can use the manual setup to specify that.

# The "Manual" variables will take precedence over the "Auto" variables, to make it easy to achieve custom behavior
# as needed.
# The "Auto" mechanism uses cool-vcpkg to fetch, build, and install the dependencies locally.

# GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet - STRING
set(GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet ""
    CACHE STRING "Default triplet for Vcpkg to use when there is not one otherwise specified."
)
# GauntGenerated_ChessColiseum_ExternalDependency_Auto_ChainLoadToolchainFilepath - FILEPATH
set(GauntGenerated_ChessColiseum_ExternalDependency_Auto_ChainLoadToolchainFilepath ""
    CACHE FILEPATH "Path to the toolchain file to use for cross-compilation."
)
# GauntGenerated_ChessColiseum_ExternalDependency_Auto_GitTag - STRING
set(GauntGenerated_ChessColiseum_ExternalDependency_Auto_GitTag ""
    CACHE STRING "Git tag to use for the external dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Fetch
    "Fetch and build the Xerces-C++ dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Version "3.3.0"
    CACHE STRING "Version of the Xerces-C++ dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the Xerces-C++ dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Features ""
    CACHE STRING "Features to enable for the Xerces-C++ dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the Xerces-C++ dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the Xerces-C++ dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the Xerces-C++ dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the Xerces-C++ dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Fetch
    "Fetch and build the cnats dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Version ""
    CACHE STRING "Vcpkg version of the cnats dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the cnats dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Features ""
    CACHE STRING "Features to enable for the cnats dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the cnats dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the cnats dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the cnats dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the cnats dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Fetch - BOOL
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Fetch
    "Fetch and build the TinyProcessLibrary dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Version ""
    CACHE STRING "Vcpkg version of the TinyProcessLibrary dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the TinyProcessLibrary dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Features ""
    CACHE STRING "Features to enable for the TinyProcessLibrary dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the TinyProcessLibrary dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the TinyProcessLibrary dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the TinyProcessLibrary dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the TinyProcessLibrary dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Fetch
    "Fetch and build the GauntCommon dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Version ""
    CACHE STRING "Vcpkg version of the GauntCommon dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the GauntCommon dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Auto_Features ""
    CACHE STRING "Features to enable for the GauntCommon dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the GauntCommon dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the GauntCommon dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the GauntCommon dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_GauntCommon_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the GauntCommon dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Fetch
    "Fetch and build the NatsCxxWrapper dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Version ""
    CACHE STRING "Vcpkg version of the NatsCxxWrapper dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the NatsCxxWrapper dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Features ""
    CACHE STRING "Features to enable for the NatsCxxWrapper dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the NatsCxxWrapper dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the NatsCxxWrapper dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the NatsCxxWrapper dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the NatsCxxWrapper dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Fetch
    "Fetch and build the spdlog dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Version ""
    CACHE STRING "Vcpkg version of the spdlog dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the spdlog dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Features ""
    CACHE STRING "Features to enable for the spdlog dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the spdlog dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the spdlog dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the spdlog dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the spdlog dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Fetch
    "Fetch and build the gtest dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Version ""
    CACHE STRING "Vcpkg version of the gtest dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the gtest dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Features ""
    CACHE STRING "Features to enable for the gtest dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the gtest dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the gtest dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the gtest dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_gtest_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the gtest dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Fetch
    "Fetch and build the tbb dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Version ""
    CACHE STRING "Vcpkg version of the tbb dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the tbb dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Auto_Features ""
    CACHE STRING "Features to enable for the tbb dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the tbb dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the tbb dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the tbb dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_tbb_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the tbb dependency."
)

# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Fetch - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Fetch
    "Fetch and build the concurrentqueue dependency automatically?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Version - VERSION
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Version ""
    CACHE STRING "Vcpkg version of the concurrentqueue dependency to use."
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_LibraryLinkage - STRING[SHARED|STATIC]
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_LibraryLinkage ""
    CACHE STRING "Library linkage for the concurrentqueue dependency. Use 'SHARED' or 'STATIC'."
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Features - list<STRING>
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Features ""
    CACHE STRING "Features to enable for the concurrentqueue dependency. Use List<STRING>."
)

# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_UseFindPackage - BOOL
option(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_UseFindPackage
    "Use FindPackage with manually specified details to find the concurrentqueue dependency?" OFF
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_Library - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_Library ""
    CACHE FILEPATH "Manually specified path to library for the concurrentqueue dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_IncludeDirectories - list<PATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_IncludeDirectories ""
    CACHE PATH "Manually specified include directories for the concurrentqueue dependency."
)
# GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_LinkLibraries - list<FILEPATH>
set(GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Manual_LinkLibraries ""
    CACHE FILEPATH "Manually specified link libraries for the concurrentqueue dependency."
)

# Helper method to check if a manual external dependency is being used, to make it easier to override auto setup.
macro(IsUsingManualExternalDependencies)

    set(oneValueArgs PACKAGE_NAME RESULT_VARIABLE)
    cmake_parse_arguments(gg_chess_coliseum_check_manual "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED gg_chess_coliseum_check_manual_PACKAGE_NAME)
        message(FATAL_ERROR "IsUsingManualExternalDependencies(PACKAGE_NAME) must be set.")
    endif()

    if (NOT DEFINED gg_chess_coliseum_check_manual_RESULT_VARIABLE)
        message(FATAL_ERROR "IsUsingManualExternalDependencies(RESULT_VARIABLE) must be set.")
    endif()

    if (GauntGenerated_ChessColiseum_ExternalDependency_${check_manual_PACKAGE_NAME}_Manual_UseFindPackage
            OR GauntGenerated_ChessColiseum_ExternalDependency_${check_manual_PACKAGE_NAME}_Manual_Library
            OR GauntGenerated_ChessColiseum_ExternalDependency_${check_manual_PACKAGE_NAME}_Manual_IncludeDirectories
            OR GauntGenerated_ChessColiseum_ExternalDependency_${check_manual_PACKAGE_NAME}_Manual_LinkLibraries
            OR GauntGenerated_ChessColiseum_ExternalDependency_${check_manual_PACKAGE_NAME}_Manual_Interpreter)
        set(${gg_chess_coliseum_check_manual_RESULT_VARIABLE} TRUE)
    else()
        set(${gg_chess_coliseum_check_manual_RESULT_VARIABLE} FALSE)
    endif()

endmacro()

macro(IsFetchingAnyExternalDependencies)

    set(oneValueArgs OUTPUT_VARIABLE)
    cmake_parse_arguments(gg_chess_coliseum_check_fetch "" "${oneValueArgs}" "" ${ARGN})

    if (NOT DEFINED gg_chess_coliseum_check_fetch_OUTPUT_VARIABLE)
        message(FATAL_ERROR "IsFetchingAnyExternalDependencies(OUTPUT_VARIABLE) must be set.")
    endif()

    if (GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Fetch
        OR GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Fetch
        OR GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Auto_Fetch
        OR GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Fetch
        OR GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Fetch
        OR GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Fetch
    )
        set(${gg_chess_coliseum_check_fetch_OUTPUT_VARIABLE} TRUE)
    else()
        set(${gg_chess_coliseum_check_fetch_OUTPUT_VARIABLE} FALSE)
    endif()

endmacro()

macro(GauntGenerated_ChessColiseum_HandleExternalDependencies)

    IsUsingManualExternalDependencies(PACKAGE_NAME XercesC              RESULT_VARIABLE gg_chess_coliseum_using_manual_XercesC)
    IsUsingManualExternalDependencies(PACKAGE_NAME cnats                RESULT_VARIABLE gg_chess_coliseum_using_manual_cnats)
    IsUsingManualExternalDependencies(PACKAGE_NAME NatsCxxWrapper       RESULT_VARIABLE gg_chess_coliseum_using_manual_NatsCxxWrapper)
    IsUsingManualExternalDependencies(PACKAGE_NAME spdlog               RESULT_VARIABLE gg_chess_coliseum_using_manual_spdlog)
    IsUsingManualExternalDependencies(PACKAGE_NAME gtest                RESULT_VARIABLE gg_chess_coliseum_using_manual_gtest)
    IsUsingManualExternalDependencies(PACKAGE_NAME concurrentqueue      RESULT_VARIABLE gg_chess_coliseum_using_manual_concurrentqueue)
    IsUsingManualExternalDependencies(PACKAGE_NAME TinyProcessLibrary   RESULT_VARIABLE gg_chess_coliseum_using_manual_TinyProcessLibrary)

    # Auto setup section
    IsFetchingAnyExternalDependencies(OUTPUT_VARIABLE gg_chess_coliseum_needs_cool_vcpkg)
    if (gg_chess_coliseum_needs_cool_vcpkg)

        message(STATUS "Fetching external dependencies with cool-vcpkg.")

        if (NOT DEFINED GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet
                OR GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet STREQUAL "")
            message(FATAL_ERROR "GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet must be set.")
        endif()

        cmake_policy(SET CMP0169 OLD)
        include(FetchContent)
        FetchContent_Declare(
                gg_chess_coliseum_cool_vcpkg_latest
                GIT_REPOSITORY https://github.com/XJ-0461/cool-vcpkg.git
                GIT_TAG "${GauntGenerated_ChessColiseum_ExternalDependency_Auto_GitTag}"
                SOURCE_SUBDIR automatic-setup
        )
        FetchContent_MakeAvailable(gg_chess_coliseum_cool_vcpkg_latest)
        cmake_policy(SET CMP0169 NEW)

        include(CoolVcpkg)
        #if (NOT DEFINED GauntGenerated_ChessColiseum_ExternalDependency_Auto_ChainLoadToolchainFilepath
        #        OR GauntGenerated_ChessColiseum_ExternalDependency_Auto_ChainLoadToolchainFilepath STREQUAL "")
        #    cool_vcpkg_SetUpVcpkg(
        #            DEFAULT_TRIPLET ${GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet}
        #            ROOT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../external/cool-vcpkg/"
        #    )
        #else()
            cool_vcpkg_SetUpVcpkg(
                    DEFAULT_TRIPLET ${GauntGenerated_ChessColiseum_ExternalDependency_Auto_DefaultTriplet}
                    ROOT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../external/cool-vcpkg/"
                    CHAIN_LOAD_TOOLCHAIN ${GauntGenerated_ChessColiseum_ExternalDependency_Auto_ChainLoadToolchainFilepath}
            )
        #endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME xerces-c
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Version}"
                LIBRARY_LINKAGE ${GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_LibraryLinkage}
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Features}"
            )
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME cnats
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Version}"
                LIBRARY_LINKAGE ${GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_LibraryLinkage}
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Features}"
            )
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME tiny-process-library
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Version}"
                LIBRARY_LINKAGE ${GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_LibraryLinkage}
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Features}"
            )
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME gtest
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Version}"
                LIBRARY_LINKAGE ${GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_LibraryLinkage}
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Features}"
            )
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME spdlog
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Version}"
                LIBARY_LINKAGE ${GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_LibraryLinkage}
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Features}"
            )
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Fetch)
            cool_vcpkg_DeclarePackage(
                NAME concurrentqueue
                VERSION "${GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Version}"
                LIBRARY_LINKAGE static
                FEATURES "${GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Features}"
                USE_DEFAULT_FEATURES OFF
            )
        endif()

        cool_vcpkg_InstallPackages()

        if (GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Auto_Fetch)
            find_package(XercesC REQUIRED)
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_cnats_Auto_Fetch)
            find_package(cnats CONFIG REQUIRED)
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Auto_Fetch)
            find_package(tiny-process-library CONFIG REQUIRED)
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_gtest_Auto_Fetch)
            find_package(GTest CONFIG REQUIRED)
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_spdlog_Auto_Fetch)
            find_package(spdlog CONFIG REQUIRED)
        endif()

        if (GauntGenerated_ChessColiseum_ExternalDependency_concurrentqueue_Auto_Fetch)
            find_package(unofficial-concurrentqueue CONFIG REQUIRED)
        endif()

    endif()

    # Manual setup section
    if (GauntGenerated_ChessColiseum_ExternalDependency_XercesC_Manual_UseFindPackage)
        find_package(XercesC REQUIRED)
    endif()

    if (GauntGenerated_ChessColiseum_ExternalDependency_cnats_Manual_UseFindPackage)
        find_package(cnats CONFIG REQUIRED)
    endif()

    if (GauntGenerated_ChessColiseum_ExternalDependency_TinyProcessLibrary_Manual_UseFindPackage)
        find_package(tiny-process-library CONFIG REQUIRED)
    endif()

    if (GauntGenerated_ChessColiseum_ExternalDependency_NatsCxxWrapper_Manual_UseFindPackage)
        find_package(nats-cxx-wrapper CONFIG REQUIRED)
    endif()

endmacro()
