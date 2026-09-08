# MobaTest.cmake
#
# One test framework project-wide (doctest), one shared main(), one way to
# declare a test binary.
#
#   moba_add_test(test_fx SOURCES test_fx.cpp test_fx64.cpp LIBS moba::fx)
#
# Each TEST_CASE becomes its own ctest entry, so a failure names the case
# rather than the binary.
#
# NOTE on selecting tests: doctest_discover_tests registers each entry under the
# TEST_CASE name ALONE. The TEST_SUITE becomes a ctest LABEL, not part of the
# name, so `ctest -R` matches case names and `ctest -L` matches suites:
#
#   ctest --preset debug -L 'fx/fx64'      whole suite
#   ctest --preset debug -R 'four-path'    one case
#
# The consequence that bites: two TEST_CASEs with the same name anywhere in the
# project collide into one ctest entry name and configure fails. That is why
# test_fx64.cpp prefixes its mirrored cases with `fx64: ` -- the two files
# deliberately test the same properties under the same names otherwise.

include(FetchContent)

function(moba_setup_test_framework)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.5.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(doctest)

    # doctest's own headers are not built under our warning set. Mark them as
    # system includes so -Werror applies to moba code only.
    get_target_property(_doctest_inc doctest INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(
        doctest
        PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_doctest_inc}"
    )

    # doctest ships a CMake helper that enumerates TEST_CASEs after the binary
    # is built and registers each one with ctest.
    if(EXISTS "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
        include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
        set(MOBA_HAVE_DOCTEST_DISCOVERY TRUE CACHE INTERNAL "")
    else()
        set(MOBA_HAVE_DOCTEST_DISCOVERY FALSE CACHE INTERNAL "")
    endif()

    # Shared main(). An OBJECT library, not STATIC: a static library's main()
    # is not referenced by anything and the linker is entitled to drop it.
    add_library(
        moba_test_main
        OBJECT
        "${CMAKE_SOURCE_DIR}/tests/doctest_main.cpp"
    )
    target_link_libraries(moba_test_main PUBLIC doctest::doctest moba::options)
    add_library(moba::test_main ALIAS moba_test_main)
endfunction()

function(moba_add_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "moba_add_test(${target}): SOURCES is required")
    endif()

    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(
        ${target}
        PRIVATE moba::test_main doctest::doctest moba::options ${ARG_LIBS}
    )

    if(MOBA_HAVE_DOCTEST_DISCOVERY)
        doctest_discover_tests(${target}
            ADD_LABELS 1
            PROPERTIES TIMEOUT 120
        )
    else()
        add_test(NAME ${target} COMMAND ${target})
    endif()
endfunction()

# A test that passes only when a translation unit FAILS to compile, for the
# specific reason named in EXPECT.
#
#   moba_add_compile_fail_test(fx_lit_hex
#       SOURCE  compile_fail/lit_hex.cpp
#       EXPECT  "only decimal digits"
#       LIBS    moba::fx
#   )
#
# EXPECT is required, and it is the whole point of the helper. The obvious
# spellings -- WILL_FAIL, or PASS_REGULAR_EXPRESSION "FAIL" against ninja's
# `FAILED:` line -- both pass on ANY compile error, so a renamed header, a
# typo in the test file, or an unrelated -Werror hit all read as green. That
# was measured, not assumed: replacing lit_hex.cpp with `int main() { this is
# not c++ ; }` left fx_lit_hex passing.
#
# What makes EXPECT work is that the rejections in fx.hpp are `throw` in a
# consteval function, and both compilers echo the throw's source line -- string
# literal included -- in the diagnostic:
#
#   clang  note:  subexpression not valid in a constant expression
#            461 |     throw "fx literal: only decimal digits ...";
#   gcc    error: expression '<throw-expression>' is not a constant expression
#            461 |     throw "fx literal: only decimal digits ...";
#
# so a substring of the message pins WHICH guard fired. Keep EXPECT free of
# regex metacharacters (no '.', no parens) and short enough to survive a
# reworded message being noticed rather than silently un-matching.
#
# Two CTest properties are doing load-bearing work:
#
#   PASS_REGULAR_EXPRESSION  documented to IGNORE the process exit code, so it
#       replaces WILL_FAIL rather than combining with it. Do not set both --
#       WILL_FAIL would invert the result of the regex match.
#   RESOURCE_LOCK  these tests invoke `cmake --build` on the build tree ctest
#       was itself launched from. Under `ctest -j` two of them would otherwise
#       run concurrent ninja invocations against one build directory. The lock
#       serialises them against each other.
#
# The target is prefixed rather than sharing the test's name, so `--target
# cf_foo` inside a test named `foo` reads unambiguously.
function(moba_add_compile_fail_test target)
    cmake_parse_arguments(ARG "" "SOURCE;EXPECT" "LIBS" ${ARGN})

    if(NOT ARG_SOURCE)
        message(FATAL_ERROR "moba_add_compile_fail_test(${target}): SOURCE is required")
    endif()
    if(NOT ARG_EXPECT)
        message(
            FATAL_ERROR
            "moba_add_compile_fail_test(${target}): EXPECT is required. A "
            "compile-fail test with no expected diagnostic passes on any "
            "compile error at all, including ones the test is not about."
        )
    endif()

    add_executable(cf_${target} EXCLUDE_FROM_ALL ${ARG_SOURCE})
    target_link_libraries(cf_${target} PRIVATE moba::options ${ARG_LIBS})

    add_test(
        NAME ${target}
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target cf_${target}
            --config $<CONFIG>
    )
    set_tests_properties(
        ${target}
        PROPERTIES
            PASS_REGULAR_EXPRESSION "${ARG_EXPECT}"
            RESOURCE_LOCK moba_build_tree
            LABELS compile_fail
    )
endfunction()
