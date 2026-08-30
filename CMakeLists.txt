cmake_minimum_required(VERSION 3.24)

project(moba VERSION 0.1.0 DESCRIPTION "moba - deterministic 1v1 arena simulation" LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Global language settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # -std=c++20, not -std=gnu++20
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for .clangd

# Predictable artefact locations regardless of generator.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(MOBA_SANITIZE "Enable ASan + UBSan" OFF)
option(MOBA_BUILD_TESTS "Build the test suite" ON)
option(MOBA_WERROR "Treat warnings as errors" ON)

set(MOBA_EXPECT_COMPILER_ID
    ""
    CACHE STRING
    "If set, configure fails unless CMAKE_CXX_COMPILER_ID matches. Guards against \
a preset silently resolving to a different compiler than its name claims."
)

include(MobaCompilerGuard)
include(MobaWarnings)

# ---------------------------------------------------------------------------
# Compile settings carried by every moba target
# ---------------------------------------------------------------------------
add_library(moba_options INTERFACE)
add_library(moba::options ALIAS moba_options)
moba_apply_compile_settings(moba_options)

# ---------------------------------------------------------------------------
# Test framework
#
# Set up before the libraries, because each library's tests/ subdirectory
# calls moba_add_test() at configure time.
# ---------------------------------------------------------------------------
if(MOBA_BUILD_TESTS)
    enable_testing()
    include(MobaTest)
    moba_setup_test_framework()
endif()

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
add_subdirectory(engine)
