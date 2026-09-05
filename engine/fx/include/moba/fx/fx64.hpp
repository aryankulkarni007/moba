#pragma once

// Overflow policy is stated once at the top of <moba/fx/fx.hpp> and applies
// here identically: abort in debug, wrap in release; the only clamp outside
// the _sat functions is division by zero.

#include <compare>
#include <moba/core/assert.hpp>
#include <moba/fx/fx.hpp>
#include <type_traits>

namespace moba {

namespace detail {

  /// does a 128-bit intermediate still fit fx64's 64-bit storage?
  [[nodiscard]] constexpr bool fits_i64(i128 v) noexcept {
    return v >= I64_MIN && v <= I64_MAX;
  }

  /// clamp a 128-bit intermediate into fx64's storage. Only for the _sat family.
  [[nodiscard]] constexpr i64 clamp_i64(i128 v) noexcept {
    if (v > I64_MAX) return I64_MAX;
    if (v < I64_MIN) return I64_MIN;
    return static_cast<i64>(v);
  }

  // __int128 is a vendor extension, so C++20's arithmetic-shift guarantee for
  // standard integer types does not formally cover it. Both compilers do
  // arithmetic; pin it rather than assume it, because operator* below depends on
  // the shift flooring the same way fx::operator* does.
  static_assert((static_cast<i128>(-1) >> 1) == -1, "i128 >> must be arithmetic");

} // namespace detail

/* fx64 -- signed Q32.32 fixed point. Scratch space, not storage.
 *
 *   storage      one i64. 32 integer bits, 32 fractional bits.
 *   scale        SCALE = 2^32. The stored integer is the value times 2^32.
 *   range        fx64::MIN .. fx64::MAX, i.e. about -2.1e9 .. +2.1e9
 *   resolution   fx64::EPSILON = 1/2^32, about 2.3e-10
 *   overflow     see the policy block in fx.hpp
 *
 * Rounding follows fx exactly, including the inconsistency documented there:
 * multiply and floor_to_int round toward negative infinity, division rounds
 * toward zero. When that is settled in fx, settle it here in the same commit. */
struct [[nodiscard]] fx64 {
  /* REPRESENTATION */

  static constexpr int SHIFT = 32;
  static constexpr i64 SCALE = i64{ 1 } << SHIFT; // 4294967296

  i64 raw = 0;

  /* CONSTRUCTION */

  // Same reason as fx: user-declared, so `fx64 a{ 4 }` does not compile.
  constexpr fx64() noexcept = default;

  static constexpr fx64 from_raw(i64 r) noexcept {
    fx64 f{};
    f.raw = r;
    return f;
  }

  // Total over i32 -- I32_MAX * 2^32 fits i64 and I32_MIN * 2^32 is exactly
  // I64_MIN -- so unlike fx::from_int this needs no range assert.
  static constexpr fx64 from_int(i32 n) noexcept { return from_raw(i64{ n } * SCALE); }

  // Total as well: |raw| << 16 maxes at 2^47.
  static constexpr fx64 widen(fx v) noexcept {
    return from_raw(i64{ v.raw } << (fx64::SHIFT - fx::SHIFT));
  }

  /* CONVERSION TO INTEGER -- same three names as fx, same three answers. */

  /// largest whole number <= value.  floor_to_int(-0.5) == -1
  [[nodiscard]] constexpr i64 floor_to_int() const noexcept { return raw >> SHIFT; }

  /// drop the fraction, toward zero.  trunc_to_int(-0.5) == 0
  [[nodiscard]] constexpr i64 trunc_to_int() const noexcept { return raw / SCALE; }

  /// nearest whole number, halves upward.  round_to_int(-0.5) == 0
  [[nodiscard]] constexpr i64 round_to_int() const noexcept {
    MOBA_ASSERT(detail::fits_i64(static_cast<i128>(raw) + (SCALE / 2)));
    return (raw + (SCALE / 2)) >> SHIFT;
  }

  /* ARITHMETIC -- assert in debug, wrap in release */

  constexpr fx64 operator+(fx64 o) const noexcept {
    MOBA_ASSERT(detail::fits_i64(static_cast<i128>(raw) + o.raw));
    return from_raw(raw + o.raw);
  }

  constexpr fx64 operator-(fx64 o) const noexcept {
    MOBA_ASSERT(detail::fits_i64(static_cast<i128>(raw) - o.raw));
    return from_raw(raw - o.raw);
  }

  // -I64_MIN is not representable, so negation is a no-op at that one input.
  constexpr fx64 operator-() const noexcept {
    MOBA_ASSERT(raw != I64_MIN);
    return from_raw(-raw);
  }

  constexpr fx64 operator*(fx64 o) const noexcept {
    // the i128 product cannot overflow; the shift back down to Q32.32 can.
    const i128 shifted = (static_cast<i128>(raw) * o.raw) >> SHIFT;
    MOBA_ASSERT(detail::fits_i64(shifted));
    return from_raw(static_cast<i64>(shifted));
  }

  // Division by zero is the policy's one exception: no wrapping answer exists,
  // so it clamps on the numerator's sign. The narrowing below is NOT an
  // exception and wraps like everything else.
  constexpr fx64 operator/(fx64 o) const noexcept {
    MOBA_ASSERT(o.raw != 0);
    if (o.raw == 0) return from_raw(raw >= 0 ? I64_MAX : I64_MIN);
    const i128 wide = (static_cast<i128>(raw) << SHIFT) / o.raw;
    MOBA_ASSERT(detail::fits_i64(wide));
    return from_raw(static_cast<i64>(wide));
  }

  /* MIXING WITH fx
   *
   * + and - widen the fx operand and stay in Q32.32. operator*(fx) is the one
   * that earns its keep: scaling a Q32.32 accumulator by a Q16.16 factor is
   * the damage-over-time shape -- a running total times a per-tick multiplier
   * -- and doing it via widen() would shift twice and round twice for nothing. */

  // clang-format off
  constexpr fx64 operator+(fx v) const noexcept { return *this + widen(v); }
  constexpr fx64 operator-(fx v) const noexcept { return *this - widen(v); }
  // clang-format on

  constexpr fx64 operator*(fx v) const noexcept {
    // Q32.32 * Q16.16 is Q48.48; shift off the fx scale to land back in Q32.32.
    const i128 shifted = (static_cast<i128>(raw) * v.raw) >> fx::SHIFT;
    MOBA_ASSERT(detail::fits_i64(shifted));
    return from_raw(static_cast<i64>(shifted));
  }

  /* SCALING BY A PLAIN INTEGER -- see the note in fx.hpp. No operator+(i32),
   * for the same reason. */

  constexpr fx64 operator*(i32 n) const noexcept {
    MOBA_ASSERT(detail::fits_i64(static_cast<i128>(raw) * n));
    return from_raw(raw * n);
  }

  constexpr fx64 operator/(i32 n) const noexcept {
    MOBA_ASSERT(n != 0);
    if (n == 0) return from_raw(raw >= 0 ? I64_MAX : I64_MIN);
    // I64_MIN / -1 is the one integer division that overflows; in C++ that is
    // UB rather than a wrap, so it has to be intercepted before the divide.
    const bool overflows = raw == I64_MIN && n == -1;
    MOBA_ASSERT(!overflows);
    if (overflows) return from_raw(I64_MIN); // the value a wrap would produce
    return from_raw(raw / n);
  }

  /* COMPOUND ASSIGNMENT
   *
   * Every form delegates to its binary operator, so the asserts live in one
   * place and `a OP= b` is `a = a OP b` by construction rather than by care. */

  // clang-format off
  constexpr fx64& operator+=(fx64 o) noexcept { *this = *this + o; return *this; }
  constexpr fx64& operator-=(fx64 o) noexcept { *this = *this - o; return *this; }
  constexpr fx64& operator*=(fx64 o) noexcept { *this = *this * o; return *this; }
  constexpr fx64& operator/=(fx64 o) noexcept { *this = *this / o; return *this; }

  constexpr fx64& operator+=(fx v)   noexcept { *this = *this + v; return *this; }
  constexpr fx64& operator-=(fx v)   noexcept { *this = *this - v; return *this; }
  constexpr fx64& operator*=(fx v)   noexcept { *this = *this * v; return *this; }

  constexpr fx64& operator*=(i32 n)  noexcept { *this = *this * n; return *this; }
  constexpr fx64& operator/=(i32 n)  noexcept { *this = *this / n; return *this; }
  // clang-format on

  // TODO: [decide] No operator/(fx). Dividing a Q32.32 total by a Q16.16
  //       factor has no caller yet, and the shift direction is the opposite of
  //       operator*(fx) so it is not a copy-paste. Add it when something needs
  //       it, not before.

  /* COMPARISON */

  constexpr std::strong_ordering operator<=>(const fx64&) const = default;

  /* CONSTANTS -- see the note in fx.hpp on why these are declared then defined
   * outside the class. */

  static const fx64 ZERO, ONE, EPSILON, MIN, MAX;
};

// clang-format off
inline constexpr fx64 fx64::ZERO    = fx64::from_raw(0);
inline constexpr fx64 fx64::ONE     = fx64::from_raw(fx64::SCALE);
inline constexpr fx64 fx64::EPSILON = fx64::from_raw(1);      // one ULP
inline constexpr fx64 fx64::MIN     = fx64::from_raw(I64_MIN);
inline constexpr fx64 fx64::MAX     = fx64::from_raw(I64_MAX);

/// `3 * v` as well as `v * 3`, and `a + b` however the operands are ordered.
[[nodiscard]] constexpr fx64 operator*(i32 n, fx64 v) noexcept { return v * n; }
[[nodiscard]] constexpr fx64 operator*(fx a, fx64 b)  noexcept { return b * a; }
[[nodiscard]] constexpr fx64 operator+(fx a, fx64 b)  noexcept { return b + a; }
// clang-format on

// Same reason as the block in fx.hpp: accumulators sit inside the game state,
// and the game state is copied and fingerprinted byte for byte.
static_assert(sizeof(fx64) == 8);
static_assert(alignof(fx64) == 8);
static_assert(std::is_trivially_copyable_v<fx64>);
static_assert(std::is_standard_layout_v<fx64>);
static_assert(std::has_unique_object_representations_v<fx64>);
static_assert(!std::is_aggregate_v<fx64>);

/* FREE FUNCTIONS -- the same five as fx, so code that works on one width reads
 * the same on the other. floor/ceil/round/frac/lerp are deliberately absent:
 * fx64 is scratch space, and nothing rounds a value that is about to be
 * narrowed anyway. Add them when something actually asks. */

/// magnitude. abs(fx64::MIN) is not representable.
[[nodiscard]] constexpr fx64 abs(fx64 v) noexcept {
  MOBA_ASSERT(v.raw != I64_MIN);
  return v.raw < 0 ? fx64::from_raw(-v.raw) : v;
}

// clang-format off
[[nodiscard]] constexpr fx64 min(fx64 a, fx64 b) noexcept { return a.raw < b.raw ? a : b; }
[[nodiscard]] constexpr fx64 max(fx64 a, fx64 b) noexcept { return a.raw > b.raw ? a : b; }
// clang-format on

/// deliberate range limiting, not overflow. See fx::clamp.
[[nodiscard]] constexpr fx64 clamp(fx64 v, fx64 lo, fx64 hi) noexcept {
  MOBA_ASSERT(lo.raw <= hi.raw);
  return min(max(v, lo), hi);
}

/// -1, 0 or +1.
[[nodiscard]] constexpr i32 sign(fx64 v) noexcept {
  return (v.raw > 0) - (v.raw < 0);
}

/* CONVERSIONS BETWEEN THE TWO WIDTHS
 *
 * fx64 is scratch space, not storage. The game state stores fx, because it is
 * copied every tick for rollback and sent over the network, and doubling every
 * number doubles both costs for range nothing needs. But some arithmetic needs
 * more room than Q16.16 while it is in flight:
 *
 *   - a * b: the exact product of two Q16.16 numbers IS Q32.32. Compute it
 *     wide, then come back down.
 *   - anything accumulated over many ticks -- damage over time, velocity times
 *     frame time. A tiny per-tick amount rounds to zero in Q16.16 and the
 *     effect silently does nothing; in Q32.32 it survives until it is worth a
 *     whole ULP.
 *
 * So: widen, do the work, narrow to store. narrow() is the trip back. */

// Exact and total: Q16.16 * Q16.16 is Q32.32 with no shift and no loss, and
// the i64 product maxes at 2^62. No check needed. This is the primitive
// fx::operator* should reuse.
[[nodiscard]] constexpr fx64 mul_wide(fx a, fx b) noexcept {
  return fx64::from_raw(i64{ a.raw } * b.raw);
}

// Wraps, like every other narrowing in the library. This is what makes
// narrow(mul_wide(a, b)) and a * b agree on out-of-range products; when narrow
// clamped and fx::operator* wrapped, the two paths returned raw 2147483647 and
// raw 1111490560 for the same inputs.
//
// TODO: [DETERMINISM] `>> 16` floors, matching fx::operator*. But nothing yet
//       proves fx::operator*, mul_wide+narrow, and fx64::operator* round the
//       same way on negatives. Three paths, three chances to disagree, and a
//       1-ULP disagreement is a desync. Highest-value test in the fx suite.
[[nodiscard]] constexpr fx narrow(fx64 v) noexcept {
  const i64 shifted = v.raw >> (fx64::SHIFT - fx::SHIFT);
  MOBA_ASSERT(detail::fits_i32(shifted));
  return fx::from_raw(static_cast<i32>(shifted));
}

/* SATURATING ARITHMETIC -- opt-in, never the default. See the block in fx.hpp.
 * These do not assert: clamping is the requested behaviour, not a bug. */

// clang-format off
[[nodiscard]] constexpr fx64 add_sat(fx64 a, fx64 b) noexcept {
  return fx64::from_raw(detail::clamp_i64(static_cast<i128>(a.raw) + b.raw));
}

[[nodiscard]] constexpr fx64 sub_sat(fx64 a, fx64 b) noexcept {
  return fx64::from_raw(detail::clamp_i64(static_cast<i128>(a.raw) - b.raw));
}

[[nodiscard]] constexpr fx64 mul_sat(fx64 a, fx64 b) noexcept {
  return fx64::from_raw(detail::clamp_i64((static_cast<i128>(a.raw) * b.raw) >> fx64::SHIFT));
}

[[nodiscard]] constexpr fx64 div_sat(fx64 a, fx64 b) noexcept {
  if (b.raw == 0) return fx64::from_raw(a.raw >= 0 ? I64_MAX : I64_MIN);
  return fx64::from_raw(detail::clamp_i64((static_cast<i128>(a.raw) << fx64::SHIFT) / b.raw));
}

/// narrow() that clamps instead of wrapping, for callers that want the edge.
[[nodiscard]] constexpr fx narrow_sat(fx64 v) noexcept {
  return fx::from_raw(detail::clamp_i32(v.raw >> (fx64::SHIFT - fx::SHIFT)));
}
// clang-format on

} // namespace moba
