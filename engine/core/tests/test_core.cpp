#include <doctest/doctest.h>
#include <moba/core/assert.hpp>
#include <moba/core/types.hpp>
#include <type_traits>

// main() comes from tests/doctest_main.cpp.

// TODO: This file wires core into the test system and proves nothing yet.
//       All static_assert, all free at runtime:
//         - every alias is the width its name claims (sizeof)
//         - every signed alias is signed, every unsigned one unsigned
//           (is_signed_v -- catches i32 pasted into the u32 line, otherwise
//           completely silent)
//         - usize and isize match each other and a pointer
//         - i128/u128 are 16 bytes, under whatever policy you pick for the
//           conditional definition
//       The determinism argument rests on these widths and nothing checks them.
//
// TODO: Once assert.hpp is finished, this file also covers MOBA_ASSERT -- see
//       the test list at the bottom of <moba/core/assert.hpp>.

/* custom assert test */ /* clang-format off */ 
// helpers
static int g_counter = 0;
static bool counting_true() { ++g_counter; return true; }
// clang-format on

TEST_CASE("MOBA_ASSERT macros are defined") {
#ifndef MOBA_ASSERT
  FAIL("MOBA_ASSERT is not defined");
#endif
#ifndef MOBA_ASSERT_MSG
  FAIL("MOBA_ASSERT_MSG is not defined");
#endif
}

TEST_CASE("true condition does not abort") {
  MOBA_ASSERT(true);
  MOBA_ASSERT_MSG(1 + 1 == 2, "math is broken");
  CHECK(true); // should be unreachable - asserts did not abort
}

TEST_CASE("macro is statement safe (dangling else)") {
  int x = 1;
  if (x) MOBA_ASSERT(true);
  else CHECK(false); // must not be attached to the assert

  if (x) MOBA_ASSERT_MSG(true, "true");
  else CHECK(false);
}

TEST_CASE("condition evaluated exactly once") {
  g_counter = 0;
  MOBA_ASSERT(counting_true());
  CHECK(g_counter == 0);

  g_counter = 0;
  MOBA_ASSERT_MSG(counting_true(), "true");
  CHECK(g_counter == 0);
}

constexpr int checked(int x) {
  MOBA_ASSERT(x > 0);
  return x * 2;
}

TEST_CASE("MOBA_ASSERT usable in constexpr functions") {
  constexpr int result = checked(1);
  CHECK(result == 2);
  static_assert(checked(10) == 20);
}

TEST_CASE("unused variable in NDEBUG does not warn") {
  int only_used_in_assert = 1;
  MOBA_ASSERT(only_used_in_assert == 1);
  // in release build, this should not warn -Wunused-variable
}

/* custom type alias (fundmental) tests */

TEST_CASE("unsigned aliased types width check") {
  CHECK(sizeof(moba::u8) == sizeof(uint8_t));
  CHECK(sizeof(moba::u16) == sizeof(uint16_t));
  CHECK(sizeof(moba::u32) == sizeof(uint32_t));
  CHECK(sizeof(moba::u64) == sizeof(uint64_t));
  CHECK(sizeof(moba::u128) == sizeof(unsigned __int128));
  CHECK(sizeof(moba::usize) == sizeof(size_t));
}

TEST_CASE("signed aliased types width check") {
  CHECK(sizeof(moba::i8) == sizeof(int8_t));
  CHECK(sizeof(moba::i16) == sizeof(int16_t));
  CHECK(sizeof(moba::i32) == sizeof(int32_t));
  CHECK(sizeof(moba::i64) == sizeof(int64_t));
  CHECK(sizeof(moba::i128) == sizeof(__int128));
  CHECK(sizeof(moba::isize) == sizeof(ptrdiff_t));
}

TEST_CASE("signed alias signedness check") {
  CHECK(std::is_signed_v<moba::i8>);
  CHECK(std::is_signed_v<moba::i16>);
  CHECK(std::is_signed_v<moba::i32>);
  CHECK(std::is_signed_v<moba::i64>);
  CHECK(std::is_signed_v<moba::i128>);
  CHECK(std::is_signed_v<moba::isize>);
}

TEST_CASE("unsigned alias unsignedness check") {
  CHECK(!std::is_signed_v<moba::u8>);
  CHECK(!std::is_signed_v<moba::u16>);
  CHECK(!std::is_signed_v<moba::u32>);
  CHECK(!std::is_signed_v<moba::u64>);
  CHECK(!std::is_signed_v<moba::u128>);
  CHECK(!std::is_signed_v<moba::usize>);
}
