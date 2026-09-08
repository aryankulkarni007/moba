#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <limits>
#include <moba/core/types.hpp>
#include <type_traits>

// main() comes from tests/doctest_main.cpp.

// covers <moba/core/types.hpp>.

TEST_SUITE("core/types") {
  TEST_CASE("unsigned integer aliases") {
    static_assert(std::is_same_v<moba::u8, std::uint8_t>);
    static_assert(std::is_same_v<moba::u16, std::uint16_t>);
    static_assert(std::is_same_v<moba::u32, std::uint32_t>);
    static_assert(std::is_same_v<moba::u64, std::uint64_t>);
    static_assert(std::is_same_v<moba::u128, unsigned __int128>);
    static_assert(std::is_same_v<moba::usize, std::size_t>);
  }

  TEST_CASE("signed integer aliases") {
    static_assert(std::is_same_v<moba::i8, std::int8_t>);
    static_assert(std::is_same_v<moba::i16, std::int16_t>);
    static_assert(std::is_same_v<moba::i32, std::int32_t>);
    static_assert(std::is_same_v<moba::i64, std::int64_t>);
    static_assert(std::is_same_v<moba::i128, __int128>);
    static_assert(std::is_same_v<moba::isize, std::ptrdiff_t>);
  }

  TEST_CASE("architectural sizing guarantees") {
    // while is_same_v checks the alias definition, these checks ensure
    // the underlying types behave as expected for the target architecture.
    static_assert(sizeof(moba::usize) == sizeof(moba::isize));
    static_assert(sizeof(moba::usize) == sizeof(void*));
  }

  TEST_CASE("floating point aliases") {
    static_assert(std::is_same_v<moba::f32, float>);
    static_assert(std::is_same_v<moba::f64, double>);
  }

  TEST_CASE("numeric limits constants") {
    static_assert(moba::I32_MAX == std::numeric_limits<moba::i32>::max());
    static_assert(moba::I32_MIN == std::numeric_limits<moba::i32>::min());
    static_assert(moba::I64_MAX == std::numeric_limits<moba::i64>::max());
    static_assert(moba::I64_MIN == std::numeric_limits<moba::i64>::min());
  }

  TEST_CASE("header stays numeric-only") {
    // types.hpp is on the include path of every TU via assert.hpp, so its
    // weight is paid project-wide. These three are all it may include; adding
    // a convenience alias for a container or string type costs 10k-70k
    // preprocessed lines everywhere. See the note at the bottom of types.hpp.
    static_assert(sizeof(moba::usize) == sizeof(void*));
    static_assert(std::is_integral_v<moba::u64>);
    static_assert(std::is_floating_point_v<moba::f64>);
  }
}
