#include <doctest/doctest.h>
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
