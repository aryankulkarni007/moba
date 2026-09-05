#pragma once

// OVERFLOW POLICY -- one rule, every operation, fx and fx64:
//
//     abort in debug, wrap in release.
//
// One exception: division by zero, where no wrapping answer exists (the
// hardware traps), so it clamps on the numerator's sign. That is the only
// clamp outside the _sat functions -- if a bare I32_MAX/I64_MIN return turns
// up anywhere else, something has drifted.
//
// Wrapping, not saturation, because saturation costs a branch per op on the
// hot path, is not associative (so it breaks the algebra tests), and hides
// bugs -- a wrapped value flips sign and shows up, a clamped one looks
// plausible. Q16.16 spans +/-32768, so overflow always means a caller bug.
//
// Saturating arithmetic is opt-in and named: add_sat / sub_sat / mul_sat /
// div_sat below, and narrow_sat in fx64.hpp. C++26 names, so migrating later
// is a delete. Deliberate range limiting is a different job again and belongs
// to the caller: clamp(v, lo, hi).
//
// The debug asserts are the ONLY overflow detection here: -fwrapv suppresses
// UBSan's signed-overflow check, and an out-of-range i64 -> i32 cast is
// implementation-defined rather than UB, so no sanitizer reports it.

#include <compare>
#include <moba/core/assert.hpp>
#include <moba/core/types.hpp>
#include <type_traits>

namespace moba {

namespace detail {

  /// does a 64-bit intermediate still fit fx's 32-bit storage?
  [[nodiscard]] constexpr bool fits_i32(i64 v) noexcept {
    return v >= I32_MIN && v <= I32_MAX;
  }

  /// clamp a 64-bit intermediate into fx's storage. Only for the _sat functions.
  [[nodiscard]] constexpr i32 clamp_i32(i64 v) noexcept {
    if (v > I32_MAX) return I32_MAX;
    if (v < I32_MIN) return I32_MIN;
    return static_cast<i32>(v);
  }

} // namespace detail

/* fx -- signed Q16.16 fixed point.
 *
 *   storage      one i32. 16 integer bits, 16 fractional bits.
 *   scale        SCALE = 65536. The stored integer is the value times 65536.
 *   range        fx::MIN .. fx::MAX, i.e. -32768 .. 32767.99998474121
 *   resolution   fx::EPSILON = 1/65536 = 0.0000152587890625, exactly
 *   whole range  from_int accepts MIN_INT .. MAX_INT (-32768 .. 32767)
 *   overflow     see the policy block at the top of this file
 *   float        none, on any path. to_double lives in <moba/fx/format.hpp>
 *
 * ROUNDING, and the one place it is currently inconsistent:
 *   multiply and floor_to_int round toward negative infinity, because both go
 *   through an arithmetic right shift. Division rounds toward ZERO, because it
 *   goes through C++ integer division. Verified:
 *       fx::from_raw(-1) * fx::from_raw(1)  -> raw -1     (floor)
 *       -fx::ONE / fx::from_int(3)          -> raw -21845 (trunc; floor -21846)
 *
 * TODO: [DETERMINISM] Pick one direction and make every operation obey it.
 *       Two directions in one type means a / b and -((-a) / b) differ by an
 *       ULP, and an ULP is a desync. Floor is the cheaper default to keep --
 *       it is what the shifts already do -- but flooring division costs a
 *       correction:
 *           i64 q = num / den;
 *           if (num % den != 0 && ((num < 0) != (den < 0))) --q;
 *       Decide BEFORE the golden hash test exists; afterwards this changes
 *       every expected value. Whichever you pick, state it here and delete
 *       this block. */
struct [[nodiscard]] fx {
  /* REPRESENTATION */

  static constexpr int SHIFT = 16;

  // the scale factor, not the value one: `x == fx::SCALE` is a type error.
  static constexpr i32 SCALE = 1 << SHIFT; // 65536

  // the whole-number range from_int accepts; outside it, n * SCALE wraps.
  static constexpr i32 MAX_INT = I32_MAX >> SHIFT; // 32767
  static constexpr i32 MIN_INT = I32_MIN >> SHIFT; // -32768

  i32 raw = 0;

  /* CONSTRUCTION */

  // User-declared, so fx is not an aggregate and `fx a{ 4 }` does not compile.
  // Without it the shortest spelling means raw 4, i.e. 4/65536 -- the exact
  // confusion from_raw and from_int are named to prevent.
  constexpr fx() noexcept = default;

  /// have internal representation, want fx
  static constexpr fx from_raw(i32 r) noexcept {
    fx f{};
    f.raw = r;
    return f;
  }

  /// have whole number, scale it up now
  static constexpr fx from_int(i32 n) noexcept {
    MOBA_ASSERT(n >= MIN_INT && n <= MAX_INT);
    return from_raw(n * SCALE);
  }

  /// exact rational, resolved at compile time: from_ratio(1, 60), (3, 4)
  ///
  /// consteval, so a zero denominator and an out-of-range result are both
  /// compile errors rather than runtime surprises -- the throw can never
  /// execute, it only makes constant evaluation fail with a readable message.
  /// Prefer this over the _fx literal: no long double, no per-architecture
  /// difference. Rounds toward zero, matching operator/.
  [[nodiscard]] static consteval fx from_ratio(i32 num, i32 den) {
    if (den == 0) throw "fx::from_ratio: zero denominator";
    const i64 wide = (static_cast<i64>(num) << SHIFT) / den;
    if (!detail::fits_i32(wide)) throw "fx::from_ratio: result out of fx range";
    return from_raw(static_cast<i32>(wide));
  }

  /* CONVERSION TO INTEGER -- three names because there are three answers, and
   * a bare to_int() hid which one you were getting. */

  // Right shift on a negative i32 is arithmetic, guaranteed by C++20
  // [expr.shift]. That is what makes this agree with operator*.
  /// largest whole number <= value.  floor_to_int(-0.5) == -1
  [[nodiscard]] constexpr i32 floor_to_int() const noexcept { return raw >> SHIFT; }

  /// drop the fraction, toward zero.  trunc_to_int(-0.5) == 0
  [[nodiscard]] constexpr i32 trunc_to_int() const noexcept { return raw / SCALE; }

  /// nearest whole number, halves upward.  round_to_int(-0.5) == 0
  [[nodiscard]] constexpr i32 round_to_int() const noexcept {
    MOBA_ASSERT(detail::fits_i32(static_cast<i64>(raw) + (SCALE / 2)));
    return (raw + (SCALE / 2)) >> SHIFT;
  }

  /* ARITHMETIC -- assert in debug, wrap in release */

  constexpr fx operator+(fx o) const noexcept {
    MOBA_ASSERT(detail::fits_i32(static_cast<i64>(raw) + o.raw));
    return from_raw(raw + o.raw);
  }

  constexpr fx operator-(fx o) const noexcept {
    MOBA_ASSERT(detail::fits_i32(static_cast<i64>(raw) - o.raw));
    return from_raw(raw - o.raw);
  }

  // -I32_MIN is not representable, so negation is a no-op at that one input.
  constexpr fx operator-() const noexcept {
    MOBA_ASSERT(raw != I32_MIN);
    return from_raw(-raw);
  }

  // TODO: [duplication] This is mul_wide() + narrow() open-coded. fx64.hpp
  //       includes fx.hpp so it cannot be reused directly. Either accept it
  //       and property-test that the two agree (that test is on the exit
  //       criteria anyway), or move the shift to a shared detail header.
  constexpr fx operator*(fx o) const noexcept {
    // both sides are 65536 too big -> widen to 64 bits then shift off one
    // factor. The i64 product maxes at 2^62, so it cannot itself overflow.
    const i64 wide = static_cast<i64>(raw) * o.raw;
    MOBA_ASSERT(detail::fits_i32(wide >> SHIFT));
    return from_raw(static_cast<i32>(wide >> SHIFT));
  }

  // Division by zero is the policy's one exception: no wrapping answer exists,
  // so it clamps on the numerator's sign. The narrowing below is NOT an
  // exception and wraps like everything else.
  constexpr fx operator/(fx o) const noexcept {
    MOBA_ASSERT(o.raw != 0);
    if (o.raw == 0) return from_raw(raw >= 0 ? I32_MAX : I32_MIN);
    // dividing cancels the scaling entirely, so scale the top first. Signed
    // left shift is well defined in C++20 [expr.shift], and |raw| << 16 maxes
    // at 2^47, so the shift itself cannot overflow i64.
    const i64 wide = (static_cast<i64>(raw) << SHIFT) / o.raw;
    MOBA_ASSERT(detail::fits_i32(wide));
    return from_raw(static_cast<i32>(wide));
  }

  /* SCALING BY A PLAIN INTEGER
   *
   * `v * 3` means three times v, not v plus 3/65536. These are cheaper AND
   * exact compared with going through from_int: one machine instruction, no
   * widening, no shift, no rounding at all.
   *
   * There is deliberately no operator+(i32) or heterogeneous comparison: `x +
   * 1` would have to mean either 1 or 1/65536, and either reading silently
   * betrays half the callers. Use fx::ONE and fx::ZERO. */

  constexpr fx operator*(i32 n) const noexcept {
    MOBA_ASSERT(detail::fits_i32(static_cast<i64>(raw) * n));
    return from_raw(raw * n);
  }

  constexpr fx operator/(i32 n) const noexcept {
    MOBA_ASSERT(n != 0);
    if (n == 0) return from_raw(raw >= 0 ? I32_MAX : I32_MIN);
    // I32_MIN / -1 is the one integer division that overflows; in C++ it is UB
    // rather than a wrap, so it has to be intercepted before the divide.
    const bool overflows = raw == I32_MIN && n == -1;
    MOBA_ASSERT(!overflows);
    if (overflows) return from_raw(I32_MIN); // the value a wrap would produce
    return from_raw(raw / n);
  }

  /* COMPOUND ASSIGNMENT
   *
   * Every form delegates to its binary operator, so the asserts live in one
   * place and `a OP= b` is `a = a OP b` by construction rather than by care. */

  // clang-format off
  constexpr fx& operator+=(fx o)  noexcept { *this = *this + o; return *this; }
  constexpr fx& operator-=(fx o)  noexcept { *this = *this - o; return *this; }
  constexpr fx& operator*=(fx o)  noexcept { *this = *this * o; return *this; }
  constexpr fx& operator/=(fx o)  noexcept { *this = *this / o; return *this; }
  constexpr fx& operator*=(i32 n) noexcept { *this = *this * n; return *this; }
  constexpr fx& operator/=(i32 n) noexcept { *this = *this / n; return *this; }
  // clang-format on

  /* COMPARISON */

  constexpr std::strong_ordering operator<=>(const fx&) const = default;

  /* CONSTANTS -- only declared here; fx is incomplete until the closing brace,
   * so the definitions follow immediately below. */

  static const fx ZERO, ONE, EPSILON, MIN, MAX;
};

// clang-format off
inline constexpr fx fx::ZERO    = fx::from_raw(0);
inline constexpr fx fx::ONE     = fx::from_raw(fx::SCALE);
inline constexpr fx fx::EPSILON = fx::from_raw(1);         // one ULP, for tolerances
inline constexpr fx fx::MIN     = fx::from_raw(I32_MIN);
inline constexpr fx fx::MAX     = fx::from_raw(I32_MAX);
// clang-format on

/// `3 * v` as well as `v * 3`. Multiplication commutes; the spelling should too.
[[nodiscard]] constexpr fx operator*(i32 n, fx v) noexcept {
  return v * n;
}

// The game state gets copied and fingerprinted as a plain block of bytes: to
// rewind a tick, and to check the two players' machines still agree. That only
// works if an fx *is* its four bytes and nothing more. If the type had spare
// bits the compiler could leave holding junk, two fx that compare equal could
// still differ byte for byte, and the check would report the players out of
// sync when they were not. These six lines are what make that impossible.
static_assert(sizeof(fx) == 4);
static_assert(alignof(fx) == 4);
static_assert(std::is_trivially_copyable_v<fx>);
static_assert(std::is_standard_layout_v<fx>);
static_assert(std::has_unique_object_representations_v<fx>);
static_assert(!std::is_aggregate_v<fx>); // pins the `fx a{ 4 }` fix above

/* FREE FUNCTIONS
 *
 * Found by ADL, so `abs(v)` works without qualification. Each one asserts the
 * same way the operators do. */

/// magnitude. abs(fx::MIN) is not representable -- there is no +32768.
[[nodiscard]] constexpr fx abs(fx v) noexcept {
  MOBA_ASSERT(v.raw != I32_MIN);
  return v.raw < 0 ? fx::from_raw(-v.raw) : v;
}

// clang-format off
[[nodiscard]] constexpr fx min(fx a, fx b) noexcept { return a.raw < b.raw ? a : b; }
[[nodiscard]] constexpr fx max(fx a, fx b) noexcept { return a.raw > b.raw ? a : b; }
// clang-format on

/// deliberate range limiting -- health floors, arena bounds. Not overflow.
[[nodiscard]] constexpr fx clamp(fx v, fx lo, fx hi) noexcept {
  MOBA_ASSERT(lo.raw <= hi.raw);
  return min(max(v, lo), hi);
}

/// -1, 0 or +1. A plain int, because the sign of a fixed-point number is not
/// itself a fixed-point number.
[[nodiscard]] constexpr i32 sign(fx v) noexcept {
  return (v.raw > 0) - (v.raw < 0);
}

/// largest whole value <= v, still an fx. floor(-0.5) == -1
[[nodiscard]] constexpr fx floor(fx v) noexcept {
  return fx::from_raw(v.raw & ~(fx::SCALE - 1));
}

/// smallest whole value >= v, still an fx
[[nodiscard]] constexpr fx ceil(fx v) noexcept {
  MOBA_ASSERT(detail::fits_i32(static_cast<i64>(v.raw) + (fx::SCALE - 1)));
  return fx::from_raw((v.raw + (fx::SCALE - 1)) & ~(fx::SCALE - 1));
}

/// nearest whole value, halves upward
[[nodiscard]] constexpr fx round(fx v) noexcept {
  MOBA_ASSERT(detail::fits_i32(static_cast<i64>(v.raw) + (fx::SCALE / 2)));
  return fx::from_raw((v.raw + (fx::SCALE / 2)) & ~(fx::SCALE - 1));
}

/// fractional part, always in [0, 1). floor(v) + frac(v) == v for every v,
/// including negatives -- frac(-0.25) is 0.75, not -0.25.
[[nodiscard]] constexpr fx frac(fx v) noexcept {
  return fx::from_raw(v.raw & (fx::SCALE - 1));
}

/// linear interpolation. Written as a + (b - a) * t rather than
/// a * (1 - t) + b * t because only this form gives lerp(a, b, ZERO) == a and
/// lerp(a, b, ONE) == b exactly, which is an exit criterion.
[[nodiscard]] constexpr fx lerp(fx a, fx b, fx t) noexcept {
  return a + (b - a) * t;
}

/* SATURATING ARITHMETIC -- opt-in, never the default.
 *
 * For an accumulator that genuinely must not wrap. These do NOT assert:
 * clamping is the requested behaviour here, not a bug being reported. When the
 * limit is a game rule rather than the storage boundary, use clamp() above. */

// clang-format off
[[nodiscard]] constexpr fx add_sat(fx a, fx b) noexcept {
  return fx::from_raw(detail::clamp_i32(static_cast<i64>(a.raw) + b.raw));
}

[[nodiscard]] constexpr fx sub_sat(fx a, fx b) noexcept {
  return fx::from_raw(detail::clamp_i32(static_cast<i64>(a.raw) - b.raw));
}

[[nodiscard]] constexpr fx mul_sat(fx a, fx b) noexcept {
  return fx::from_raw(detail::clamp_i32((static_cast<i64>(a.raw) * b.raw) >> fx::SHIFT));
}

[[nodiscard]] constexpr fx div_sat(fx a, fx b) noexcept {
  if (b.raw == 0) return fx::from_raw(a.raw >= 0 ? I32_MAX : I32_MIN);
  return fx::from_raw(detail::clamp_i32((static_cast<i64>(a.raw) << fx::SHIFT) / b.raw));
}
// clang-format on

// TODO: [DETERMINISM] This literal goes through long double: 80-bit on x86-64,
//       128-bit quad on AArch64. Same source literal can give different raw
//       values per target, and those raws go into every golden hash. Will not
//       show up here -- both presets are the same compiler on one arch.
//
//       Two more defects in the same three lines:
//         - static_cast truncates toward zero; operator* floors. -0.1_fx gives
//           raw -6553, flooring gives -6554. Literals and arithmetic disagree.
//         - `1_fx` does not compile; a long double UDL only matches floating
//           literals.
//
//       Keep one property when rewriting: consteval makes an out-of-range
//       literal a COMPILE error (100000.0_fx is rejected), unlike from_int
//       which wraps silently at runtime.
//
//       Fix: take const char*, consteval, parse digits as integers. Exact by
//       construction, `1_fx` and `1.5_fx` both work, you choose the rounding.
//       Until then from_ratio(num, den) above is the exact, portable spelling
//       and should be preferred everywhere it fits.
consteval fx operator""_fx(long double v) {
  return fx::from_raw(static_cast<i32>(v * fx::SCALE));
}

// DECIDED: no operator double() here, ever. to_double(fx) lives in
// <moba/fx/format.hpp> instead, so the include graph keeps floating point out
// of sim TUs with nothing to remember and no macro to get wrong.

// TODO: [decide] A `fixed_point` concept over fx and fx64. abs/min/max/clamp/
//       sign now exist for both types, so the second copy the earlier note was
//       waiting for has arrived -- drift between the two IS the
//       rounding-disagreement bug class. Do it before adding a third.

// TODO: [sequencing] Dimensional units on fx (metres vs seconds vs damage).
//       Not the same job as type-safe IDs -- those are in, see
//       <moba/core/strong_id.hpp>. Difference is arithmetic closure: an id has
//       no arithmetic to define; a unit-typed scalar needs an exponent algebra
//       because metres/seconds is a third type.
//       Revisit end of phase 2, when the real unit set is known rather than
//       guessed. Free thing to do now: keep named quantities in signatures --
//       one `fx radius` converts later, four bare fx in a row does not.

} // namespace moba
