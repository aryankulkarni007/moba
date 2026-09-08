# MobaTest.cmake
#
# One test framework project-wide (doctest), one shared main(), one way to
# declare a test binary.
#
#   moba_add_test(test_fx SOURCES test_fx.cpp test_fx64.cpp LIBS moba::fx)
#
# Each TEST_CASE becomes its own ctest entry, so `ctest -R "fx::mul"` works and
# a failure names the case rather than the binary.

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

function(moba_add_compile_fail_test target source)
    add_executable(i${target} EXCLUDE_FROM_ALL ${source})
    target_link_libraries(i${target} PRIVATE moba::options ${ARGN})
    add_test(
        NAME ${target}
        COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target i${target}
            --config $<CONFIG>
    )
    set_tests_properties(${target} PROPERTIES PASS_REGULAR_EXPRESSION "FAIL")
endfunction()
