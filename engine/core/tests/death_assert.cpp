// Death tests for <moba/core/assert.hpp>.
//
// NOT a doctest file, and deliberately not linked against moba::test_main.
// Every interesting case here ends in std::abort(), which takes the whole
// process with it, so these cannot share a binary with anything else.
//
// Driven from CMake by moba_add_death_test() in cmake/MobaTest.cmake. Each
// ctest entry runs `cmake -P cmake/MobaDeathTest.cmake`, which runs THIS
// executable as a child and inspects how it died. The indirection exists
// because CTest may fail a test on SIGABRT regardless of
// PASS_REGULAR_EXPRESSION -- see the header of MobaDeathTest.cmake.
//
// The case is chosen by argv[1] rather than by building six executables,
// because each one can only die once.
//
// WHY THE HELPERS HAVE SUCH SPECIFIC NAMES: assertion_failed reports
// source_location::function_name(), which gives a full signature. A helper
// called `f` would produce output that matches almost anything; these names
// let a test assert the location machinery works rather than just that
// something printed. Renaming one breaks its test, which is the point.

#include <cstdio>
#include <cstring>
#include <moba/core/assert.hpp>

namespace {

// The stringified condition `1 == 2` is itself an expected output. The header
// notes that MOBA_ASSERT_MSG lost its condition check once already; nothing
// pinned it until now.
void trips_a_false_assert() { MOBA_ASSERT(1 == 2); }

// The message must be findable in the output and appear nowhere else in this
// file, so a test matching it cannot match by accident.
void trips_a_false_assert_with_message() {
  MOBA_ASSERT_MSG(1 == 2, "moba-death-test-sentinel-message");
}

// The controls. Without these, an implementation that aborted unconditionally
// would pass every test above.
void trips_no_assert() { MOBA_ASSERT(1 == 1); }
void trips_no_assert_with_message() {
  MOBA_ASSERT_MSG(1 == 1, "this message must never be printed");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: death_assert <case>\n");
    return 2;
  }

  const char* which = argv[1];

  if (std::strcmp(which, "assert_false") == 0) {
    trips_a_false_assert();
  } else if (std::strcmp(which, "assert_msg") == 0) {
    trips_a_false_assert_with_message();
  } else if (std::strcmp(which, "assert_true") == 0) {
    trips_no_assert();
  } else if (std::strcmp(which, "assert_msg_true") == 0) {
    trips_no_assert_with_message();
  } else {
    std::fprintf(stderr, "death_assert: unknown case '%s'\n", which);
    return 2;
  }

  // Reached in two situations, and the driver tells them apart by build type:
  //   - a control case, in either arm
  //   - an abort case under NDEBUG, where the macro compiles to nothing
  return 0;
}
