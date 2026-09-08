#pragma once

// moba/fx/isqrt.hpp -- integer square root.
//
//   isqrt(u64) -> u32    floor(sqrt(n)). Exact, total, no floating point.
//   sqrt(fx)   -> fx     the fixed-point wrapper over it
//
// isqrt is the primitive, not the other way round: vec2::length hands it a
// Q32.32 raw reaching 2^63, which does not fit through an fx parameter.
//
// SCALING -- integer sqrt consumes Q32.32 and produces Q16.16, because a root
// halves the scale factor too. isqrt(fx_raw) would give sqrt(v)*2^8, wrong by
// 2^8; feeding v*2^32 gives sqrt(v)*2^16. That widening IS fx64::widen, so:
//
//     sqrt(fx v)        isqrt(widen(v).raw)      widen first
//     vec2::length(v)   isqrt(length_sq(v).raw)  already Q32.32, no shift
//
// which is the real reason length_sq returns fx64.
//
// RANGE -- sqrt(fx) is total on non-negative input and needs no result check:
// worst case isqrt(I32_MAX << 16) = 11863283, 181x inside i32. isqrt alone is
// not so bounded (isqrt(UINT64_MAX) = 2^32-1), hence u32 out, and only sqrt()
// may narrow to i32.
//
// NEGATIVE INPUT -- assert, then return ZERO, neither inside an #if. The
// fallback exists for the RELEASE arm, which is the one with no assert.
//
// DETERMINISM -- nothing special is needed. Integer shifts, adds and compares
// are fully specified and identical everywhere; this is deterministic
// structurally. Only floating point or UB (shift count outside [0,width))
// would break it -- NOT a loop condition, which cannot differ between
// compilers.
//
// 1 << 62 AND 32 ITERATIONS -- bit must be a power of FOUR: it is the 4^k term
// of (r + 2^k)^2 = r^2 + 2*r*2^k + 4^k. From 4^31 the 32 iterations walk
// 4^31..4^0 and bit reaches 0 exactly as the loop ends, one iteration per
// result bit. Starting too HIGH is harmless (the test fails until bit drops
// into range); starting too low, or on an odd power of two, is not. That
// asymmetry is why a fixed maximum start needs no setup loop.
//
// TESTING -- postcondition r*r <= n && (r+1)*(r+1) > n, computed in u128 since
// (r+1)^2 reaches 2^64. Cover perfect squares exactly, k^2 +/- 1, powers of
// two and four, 0, 1, UINT64_MAX, and both directions of the scaling rule.
//
// TODO: [missing] Callers: vec2::length, vec2::normalise. If a third appears,
//       check whether it wants a squared comparison instead -- dist_sq against
//       mul_wide(r, r) needs no root at all, and hitboxes run per pair per
//       tick.

#include <moba/core/types.hpp>
#include <moba/fx/fx.hpp>
#include <moba/fx/fx64.hpp>

namespace moba {
[[nodiscard]] constexpr u32 isqrt(u64 n) noexcept;

[[nodiscard]] constexpr fx sqrt(fx n) noexcept {
  MOBA_ASSERT(n.raw >= 0);
  if (n.raw <= 0) return fx::ZERO;

  auto raw = fx64::widen(n).raw;
  return fx::from_raw(static_cast<i32>(isqrt(static_cast<u64>(raw))));
}

[[nodiscard]] constexpr u32 isqrt(u64 n) noexcept {
  u64 op  = n;
  u64 res = 0;

  u64 bit = u64{ 1 } << 62;

  for (usize i = 0; i < 32; ++i) {
    u64 dltasqr = res + bit;
    if (op >= dltasqr) {
      op -= dltasqr;
      res += bit << 1;
    }
    res >>= 1;
    bit >>= 2;
  }

  return static_cast<u32>(res);
}
}  // namespace moba
