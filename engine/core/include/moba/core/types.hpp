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

// TARGET GUARANTEES
//
// The aliases above are spellings, not promises. <cstdint> guarantees i32
// is 32 bits, but nothing here guarantees a byte is 8 bits, that usize is
// pointer-sized, or that __int128 is really 128 wide.
//
// these belong in the header, not in test_types.cpp: a test only fails on a
// target the tests are run on, a static_assert fails on any target the project
// compiles for.
//
// digits rather than sizeof, because digits counts value bits and so catches
// padding and an odd byte width that sizeof would report as fine. Signed types
// exclude the sign bit, hence the odd numbers.
static_assert(std::numeric_limits<u8>::digits == 8, "a byte must be 8 bits");

static_assert(std::numeric_limits<i8 >::digits ==  7 &&  std::numeric_limits<i8 >::is_signed);
static_assert(std::numeric_limits<i16>::digits == 15 &&  std::numeric_limits<i16>::is_signed);
static_assert(std::numeric_limits<i32>::digits == 31 &&  std::numeric_limits<i32>::is_signed);
static_assert(std::numeric_limits<i64>::digits == 63 &&  std::numeric_limits<i64>::is_signed);

static_assert(std::numeric_limits<u8 >::digits ==  8 && !std::numeric_limits<u8 >::is_signed);
static_assert(std::numeric_limits<u16>::digits == 16 && !std::numeric_limits<u16>::is_signed);
static_assert(std::numeric_limits<u32>::digits == 32 && !std::numeric_limits<u32>::is_signed);
static_assert(std::numeric_limits<u64>::digits == 64 && !std::numeric_limits<u64>::is_signed);

// C++20 mandates both, so neither can fire on a conforming compiler. They are
// here because fx.hpp's rounding policy rests on ">> is a floor" and on I32_MIN
// having no positive twin; a target that broke either would desync by one ULP
// rather than fail visibly.
static_assert(I32_MIN + I32_MAX == -1, "i32 must be two's complement");
static_assert(I64_MIN + I64_MAX == -1, "i64 must be two's complement");
static_assert((i32{ -1 } >> 1) == -1, "i32 >> must be arithmetic");
static_assert((i64{ -1 } >> 1) == -1, "i64 >> must be arithmetic");

// numeric_limits need not be specialised for __int128 outside GNU mode, and
// this builds with -std=c++20 rather than gnu++20, so these use sizeof and a
// sign probe. fx64.hpp separately checks that i128 >> is arithmetic, which the
// standard does not cover for a vendor extension.
static_assert(sizeof(i128) == 16 && sizeof(u128) == 16);
static_assert(static_cast<i128>(-1) < 0, "i128 must be signed");
static_assert(static_cast<u128>(-1) > 0, "u128 must be unsigned");

// usize/isize are the one pair allowed to differ per target. Nothing in the sim
// may store one, since a snapshot has to mean the same thing on both machines,
// so only the relationship between them is fixed here.
static_assert(sizeof(usize) == sizeof(void*));
static_assert(sizeof(isize) == sizeof(usize));
static_assert(!std::numeric_limits<usize>::is_signed);
static_assert(std::numeric_limits<isize>::is_signed);

// IEEE-754 binary32/binary64. Tooling and the renderer only; see below.
static_assert(std::numeric_limits<f32>::is_iec559 && sizeof(f32) == 4);
static_assert(std::numeric_limits<f64>::is_iec559 && sizeof(f64) == 8);

// NUMERIC ALIASES ONLY. Nothing else belongs in this header.

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
