# MobaCompilerGuard.cmake
#
# Cross-compiler hash agreement is the phase 0 determinism test, and it is
# worthless if a preset resolves to a different compiler than its name claims.
# Two ways that happens in practice:
#
#   /usr/bin/g++     on macOS is Apple Clang, not GCC
#   clang++          may be Apple Clang or Homebrew LLVM depending on PATH
#
# Presets set MOBA_EXPECT_COMPILER_ID; a mismatch fails configure loudly.
# CMake reports these as: Clang (LLVM), AppleClang, GNU.

if(
    MOBA_EXPECT_COMPILER_ID
    AND NOT CMAKE_CXX_COMPILER_ID STREQUAL MOBA_EXPECT_COMPILER_ID
)
    if(MOBA_EXPECT_COMPILER_ID STREQUAL "GNU")
        set(_hint
            "On macOS /usr/bin/g++ and /usr/bin/gcc are Apple Clang. Install real GCC
  (brew install gcc) and point the preset at the versioned binary,
  e.g. g++-15."
        )
    elseif(MOBA_EXPECT_COMPILER_ID STREQUAL "Clang")
        set(_hint
            "Expected upstream LLVM Clang. Apple Clang reports AppleClang and is a
  different compiler with different codegen, so it is not a substitute here.
  Install LLVM (brew install llvm) and put it ahead of /usr/bin on PATH, or
  point the preset at an absolute path, e.g.
  /opt/homebrew/opt/llvm/bin/clang++."
        )
    else()
        set(_hint "Point the preset at a compiler whose CMake id matches.")
    endif()

    message(
        FATAL_ERROR
        "Compiler identity mismatch.\n"
        "  expected : ${MOBA_EXPECT_COMPILER_ID}\n"
        "  actual   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
        "  binary   : ${CMAKE_CXX_COMPILER}\n"
        "\n"
        "  ${_hint}\n"
        "\n"
        "  Until this matches, the preset proves nothing about cross-compiler\n"
        "  determinism. Override with -DMOBA_EXPECT_COMPILER_ID= to bypass, but a\n"
        "  golden hash produced that way is not comparable with any other."
    )
endif()

message(
    STATUS
    "moba compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)
