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

// NUMERIC ALIASES ONLY. Nothing else belongs in this header.
//
// This sits at the bottom of the include graph -- fx.hpp and strong_id.hpp
// include it directly, and everything that touches a number in the sim reaches
// it through one of those -- so whatever lands here is paid for by the whole
// project. The three includes above cost ~4300 preprocessed lines between
// them. A single convenience alias undoes that:
//
//     <span>          71751        <utility>       10113
//     <array>         48524        <memory>        33379
//     <string_view>   48411        <vector>        65969
//     <string>        51884
//
// Carrying string_view/span/array/pair here put a sim TU at 72189 lines and
// 235 ms of front-end time. Numerics alone: 10893 lines, 68 ms. At five TUs
// that is noise; at two hundred it is a 47-second cold build against 14.
//
// Two separate reasons the owning types (string, vector, unique_ptr,
// shared_ptr, weak_ptr) never come back here even if the cost were free:
// World is copied and fingerprinted as a plain block of bytes -- see the
// static_assert block in <moba/fx/fx.hpp> -- and every one of them either
// heap-allocates, so the bytes in the struct are an address rather than the
// data, or stores a pointer that moves run to run under ASLR. Checksumming
// either reports a desync that did not happen, or hides one that did.
// uintptr_t/intptr_t are the same hazard with the pointer made explicit.
//
// Outside the sim -- tooling, asset loading, the eventual renderer -- spell
// std::string and std::vector in full and include the real header there. The
// friction is the point: it marks where the snapshotted world ends.
} // namespace moba
