#include <doctest/doctest.h>
#include <moba/core/assert.hpp>

// main() comes from tests/doctest_main.cpp.

// covers <moba/core/assert.hpp>.

namespace {

int g_eval_counter = 0;

bool counting_true() {
  ++g_eval_counter;
  return true;
}
bool unused_except_in_assert() { return true; }

constexpr int checked_multiply(int x) {
  MOBA_ASSERT(x > 0);
  return x * 2;
}

}  // namespace

TEST_SUITE("core/assert") {
  TEST_CASE("macros are defined") {
#if !defined(MOBA_ASSERT) || !defined(MOBA_ASSERT_MSG)
    FAIL("MOBA_ASSERT or MOBA_ASSERT_MSG is not defined");
#else
    CHECK_MESSAGE(true, "macros export success");
#endif
  }

  TEST_CASE("true conditions do not abort") {
    MOBA_ASSERT(true);
    MOBA_ASSERT_MSG(1 + 1 == 2, "math is broken");
    // if execution reaches here,
    // the assertions correctly ignored true conditions
    CHECK(true);
  }

  TEST_CASE("statement safe (dangling else)") {
    int  x            = 1;
    bool reached_else = false;

    if (x) MOBA_ASSERT(true);
    else reached_else = true;
    CHECK_FALSE(reached_else);

    if (x) MOBA_ASSERT_MSG(true, "true message");
    else reached_else = true;
    CHECK_FALSE(reached_else);
  }

  TEST_CASE(
      "condition evaluated exactly once (or zero in "
      "NDEBUG)"
  ) {
    g_eval_counter = 0;
    MOBA_ASSERT(counting_true());

#if defined(NDEBUG)
    CHECK(g_eval_counter == 0);
#else
    CHECK(g_eval_counter == 1);
#endif

    g_eval_counter = 0;
    MOBA_ASSERT_MSG(counting_true(), "testing eval count");

#if defined(NDEBUG)
    CHECK(g_eval_counter == 0);
#else
    CHECK(g_eval_counter == 1);
#endif
  }

  TEST_CASE("usable in constexpr contexts") {
    constexpr int result = checked_multiply(1);
    CHECK(result == 2);

    static_assert(checked_multiply(10) == 20);
  }

  TEST_CASE("compiler warning suppressions") {
    // proves note 1 from assert.hpp:
    // local variables used only in asserts should not trigger -Wunused-variable
    int only_used_in_assert = 42;
    MOBA_ASSERT(only_used_in_assert == 42);

    // Proves Note 1 from assert.hpp:
    // Static functions used only in asserts should not trigger
    // -Wunneeded-internal-declaration
    MOBA_ASSERT(unused_except_in_assert());

    // This test "passes" simply by compiling cleanly under -Werror in an NDEBUG
    // build.
    CHECK(true);
  }
}
