#pragma once
/* clang-format off */
#include <cstddef>
#include <cstdint>
#include <limits>

namespace moba {
using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;

using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

using f32   = float;
using f64   = double;

inline constexpr i32 I32_MAX = std::numeric_limits<i32>::max();
inline constexpr i32 I32_MIN = std::numeric_limits<i32>::min();
inline constexpr i64 I64_MAX = std::numeric_limits<i64>::max();
inline constexpr i64 I64_MIN = std::numeric_limits<i64>::min();

// 128-bit is required, not optional: fx64::operator* has no fallback. Hard
// #error here rather than a MOBA_HAVE_INT128 flag every consumer must test.
#if defined(__SIZEOF_INT128__)
using i128  = __int128;
using u128  = unsigned __int128;
#else
#error "fx64 multiply requires 128-bit integer support (GCC/Clang)"
#endif
} // namespace moba
