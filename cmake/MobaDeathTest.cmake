# MobaDeathTest.cmake
#
# RUN WITH `cmake -P`, NEVER include()d. It is a script, not a module.
#
# Why this file exists at all. A death test asserts that a process dies, and
# the obvious spelling -- run the executable directly as the ctest COMMAND and
# match its stderr -- does not work. From `cmake --help-property
# PASS_REGULAR_EXPRESSION`, and the same note on WILL_FAIL:
#
#     System-level test failures including segmentation faults, signal abort,
#     or heap errors may fail the test even if PASS_REGULAR_EXPRESSION is
#     matched.
#
# std::abort() raises SIGABRT, so CTest is entitled to fail the test whatever
# the regex says. The WILL_FAIL docs give the fix directly: "use a CMake
# command to wrap the executable run." That is this file. The signal is
# absorbed by execute_process here, inside a child of the wrapper; the process
# CTest actually watches is a plain `cmake` that exits 0 or 1.
#
# Three checks, in order of what they catch:
#
#   1. expected an abort, got a clean exit   -- the assert did not fire
#   2. expected a clean exit, got a death    -- the release arm is not silent
#   3. died, but the output does not contain EXPECT_TEXT
#
# (3) is the one that matters, and it is the same lesson as the compile-fail
# tests: "the process exited non-zero" passes on ANY crash. A segfault in
# unrelated code, an ASan report, a std::terminate from somewhere else -- all
# non-zero, all green, none of them the assert firing. Matching the message is
# what makes the test about the thing it claims to be about.
#
# string(FIND) rather than a regex, deliberately: EXPECT_TEXT is a stringified
# C++ condition, so it is full of characters a regex would need escaped --
# `==`, parentheses, `*`, `[`. Plain substring matching sidesteps all of it,
# and it survives a sanitizer prepending its own noise to the output.
#
# Inputs, all passed as -D before -P:
#   TEST_EXE      absolute path to the death-test executable
#   TEST_ARG      argv[1], selecting which case to run
#   EXPECT_ABORT  ON  -> the child must die
#                 OFF -> the child must exit 0
#   EXPECT_TEXT   substring the output must contain. Only checked when
#                 EXPECT_ABORT is ON: under NDEBUG there is no output to match.
#   FORBID_TEXT   substring the output must NOT contain. Used for the absolute
#                 source root, which pins -fmacro-prefix-map in
#                 MobaWarnings.cmake -- nothing else tests that, and a
#                 regression leaks a developer's home directory into any
#                 assert message a player might send you.

if(NOT DEFINED TEST_EXE)
    message(FATAL_ERROR "MobaDeathTest: TEST_EXE is required")
endif()
if(NOT DEFINED TEST_ARG)
    message(FATAL_ERROR "MobaDeathTest: TEST_ARG is required")
endif()

execute_process(
    COMMAND "${TEST_EXE}" "${TEST_ARG}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

# Both streams, because a sanitizer may split its report across them and
# because relying on stdio buffering order in a process that is about to abort
# is not something to depend on.
set(combined "${out}${err}")

# RESULT_VARIABLE is an integer exit code for a normal exit, but a STRING
# describing the condition when the child is killed by a signal ("Subprocess
# aborted", or similar, depending on platform). So compare as a string: only
# a genuine clean exit is "0", and every other outcome -- code or description
# -- is not.
if(rc STREQUAL "0")
    set(_died OFF)
else()
    set(_died ON)
endif()

# One place to build the failure report, so every failure names the exit
# status and shows what the child actually printed. A death test that fails
# with only "did not match" is nearly useless in CI.
function(_moba_fail reason)
    message(
        FATAL_ERROR
        "death test FAILED: ${reason}\n"
        "  executable    : ${TEST_EXE}\n"
        "  case          : ${TEST_ARG}\n"
        "  expected      : ${_expected_desc}\n"
        "  exit status   : ${rc}\n"
        "  expected text : ${EXPECT_TEXT}\n"
        "  ---- child output ----\n"
        "${combined}"
        "  ----------------------\n"
    )
endfunction()

if(EXPECT_ABORT)
    set(_expected_desc "abort, with \"${EXPECT_TEXT}\" in the output")

    if(NOT _died)
        _moba_fail(
            "the child exited cleanly. The assertion did not fire in a build "
            "where it should have -- check that NDEBUG is genuinely undefined "
            "for this preset."
        )
    endif()

    if(DEFINED EXPECT_TEXT AND NOT EXPECT_TEXT STREQUAL "")
        string(FIND "${combined}" "${EXPECT_TEXT}" _found)
        if(_found EQUAL -1)
            _moba_fail(
                "the child died, but for the WRONG REASON. It never printed "
                "the expected text, so this was some other crash -- a "
                "segfault, a sanitizer report, an unrelated abort -- and not "
                "the assertion under test."
            )
        endif()
    endif()
else()
    set(_expected_desc "clean exit (status 0)")

    if(_died)
        _moba_fail(
            "the child died when it should have exited cleanly. Under NDEBUG "
            "MOBA_ASSERT compiles to `(void)(false && (cond))` and must never "
            "abort; in a debug build this case is a control whose condition is "
            "true. Either way, dying here means the macro is evaluating "
            "something it should not."
        )
    endif()
endif()

# Applies in both arms. Vacuous when there is no output, which is the NDEBUG
# case, and that is fine -- the check costs nothing and the arm that produces
# output is the one that can leak.
if(DEFINED FORBID_TEXT AND NOT FORBID_TEXT STREQUAL "")
    string(FIND "${combined}" "${FORBID_TEXT}" _leaked)
    if(NOT _leaked EQUAL -1)
        _moba_fail(
            "the output contains the absolute source root \"${FORBID_TEXT}\". "
            "-fmacro-prefix-map in MobaWarnings.cmake is supposed to strip it "
            "from __FILE__ and source_location::file_name(), leaving "
            "repo-relative paths. Either the flag was dropped or the compiler "
            "stopped honouring it for source_location."
        )
    endif()
endif()
