moba_add_test(test_assert
    SOURCES test_assert.cpp
    LIBS moba::core
)

moba_add_test(test_types
    SOURCES test_types.cpp
    LIBS moba::core
)

moba_add_test(test_strong_id
    SOURCES test_strong_id.cpp
    LIBS moba::core
)

# Death tests for MOBA_ASSERT. A plain executable, NOT a moba_add_test: every
# interesting case here ends in abort(), so it cannot share a process with
# doctest or with the other cases. See death_assert.cpp and MobaDeathTest.cmake.
add_executable(death_assert death_assert.cpp)
target_link_libraries(death_assert PRIVATE moba::core moba::options)

# Each of these asserts a DIFFERENT substring, which is why they are separate
# entries rather than one test with a list: EXPECT is a single substring, and
# the point of each is which part of the handler's output it pins.
#
#   "assert failed at %s:%u in %s: %s: %s\n"
#                     |  |     |   |   |
#                     |  |     |   |   `-- msg          death_assert_message
#                     |  |     |   `------ #cond        death_assert_condition
#                     |  |     `---------- function     death_assert_function
#                     |  `---------------- line
#                     `------------------- file         death_assert_file
#
# Every one of them also gets the -fmacro-prefix-map check for free, via
# FORBID_TEXT in moba_add_death_test.

# The regression the header warns about by name: "that is how _MSG lost its
# condition check the first time". The macros stringify #cond, and until now
# nothing checked it arrived.
moba_add_death_test(death_assert_reports_the_condition
    EXE    death_assert
    ARG    assert_false
    EXPECT "1 == 2"
)

moba_add_death_test(death_assert_reports_the_message
    EXE    death_assert
    ARG    assert_msg
    EXPECT "moba-death-test-sentinel-message"
)

# source_location::current() as a DEFAULT ARGUMENT captures the call site, not
# the definition of assertion_failed. These two pin that it resolves to the
# macro's use site inside the helper, which is the whole reason the header
# chose source_location over __FILE__/__func__.
moba_add_death_test(death_assert_names_the_function
    EXE    death_assert
    ARG    assert_false
    EXPECT "trips_a_false_assert"
)

moba_add_death_test(death_assert_names_the_file
    EXE    death_assert
    ARG    assert_false
    EXPECT "death_assert.cpp"
)

# The controls. A true condition must exit cleanly in BOTH arms. Without
# these, an assertion_failed that fired unconditionally would pass everything
# above. MOBA_ASSERT_MSG gets its own because it is the primitive -- MOBA_ASSERT
# defers to it, so it is the one that can lose its condition check.
moba_add_death_test(death_assert_true_does_not_abort
    EXE          death_assert
    ARG          assert_true
    NEVER_ABORTS
)

moba_add_death_test(death_assert_msg_true_does_not_abort
    EXE          death_assert
    ARG          assert_msg_true
    NEVER_ABORTS
)
