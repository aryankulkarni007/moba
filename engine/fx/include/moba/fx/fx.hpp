#pragma once

// OVERFLOW POLICY -- one rule, every operation, fx and fx64:
//
//     abort in debug, wrap in release.
//
// A clamped value only where wrapping does not exist: division by zero, and
// lossy narrowing (narrow(), the casts in operator* and operator/).
//
// Wrapping, not saturation, because saturation costs a branch per op on the
// hot path, is not associative (so it breaks the algebra tests), and hides
// bugs -- a wrapped value flips sign and shows up, a clamped one looks
// plausible. Q16.16 spans +/-32768, so overflow always means a caller bug.
//
// Deliberate clamping is the caller's job: clamp(v, lo, hi) in sim code.
// Opt-in saturating arithmetic gets named functions -- add_sat / sub_sat /
// mul_sat / div_sat, the C++26 names.
//
// The debug asserts are the ONLY overflow detection here: -fwrapv suppresses
// UBSan's signed-overflow check, and an out-of-range i64 -> i32 cast is
// implementation-defined rather than UB, so no sanitizer reports it.

#include <compare>
#include <moba/core/assert.hpp>
#include <moba/core/types.hpp>

namespace moba {
// [[nodiscard]] sits on the type, so it covers every fx-returning function
// written later too. Compound assignment returns fx& and stays usable as a
// statement.
struct [[nodiscard]] fx {

  // TODO: [missing] No stated contract. Add a block comment: representation
  //       (Q16.16, i32, scale 2^16), exact min/max, resolution 1/65536,
  //       rounding = toward -inf, no floating point on any path. For overflow
  //       point at the policy block at the top of this file rather than
  //       restating it.

  static constexpr int SHIFT = 16;

  // the scale factor, not the value one: `x == fx::SCALE` is a type error.
  static constexpr i32 SCALE = 1 << SHIFT; // 65536

  // TODO: [missing] fx constants ZERO, ONE, MIN, MAX, EPSILON (= from_raw(1)).
  //       EPSILON is a prerequisite for the ULP-tolerance tests.

  i32 raw = 0;

  constexpr fx() noexcept = default;
  /// have internal representation, want fx
  // clang-format off
  static constexpr fx from_raw(i32 r) noexcept { fx f{}; f.raw = r; return f; }

  // TODO: [BUG] Silent overflow, policy violation. `n * SCALE` is i32 * i32, so
  //       the range is only +/-32767. from_int(40000).to_int() == -25536 with
  //       no diagnostic. Needs the debug assert; release keeps wrapping.
  //       Expose MIN_INT / MAX_INT so callers can see the limit.
  /// have whole number, scale it up now
  static constexpr fx from_int(i32 n) noexcept { fx f{}; f.raw = n * SCALE; return f; }
  // clang-format on

  // Right shift on a negative i32 is arithmetic, guaranteed by C++20
  // [expr.shift]. That is what makes to_int and operator* agree on floor
  // rounding across compilers.
  //
  // TODO: [naming] Floors, does not truncate toward zero: to_int(-0.5) == -1.
  //       Consistent with operator*, but the name hides it. Split into
  //       floor_to_int / round_to_int / trunc_to_int.
  /// whole number part, throwing away fractional part (rounding down)
  [[nodiscard]] constexpr i32 to_int() const noexcept { return raw >> SHIFT; }

  /* ARITHMETIC */

  // Wrapping here is the policy, not an oversight.
  //
  // TODO: [missing] Debug asserts; these three have no overflow check at all.
  //       Unary minus is the sharp one: -from_raw(I32_MIN) returns I32_MIN
  //       unchanged (verified), so -(-a) == a is false at that value.
  // clang-format off
  constexpr fx operator+(fx o) const noexcept { return from_raw(raw + o.raw); }
  constexpr fx operator-(fx o) const noexcept { return from_raw(raw - o.raw); }
  constexpr fx operator-()     const noexcept { return from_raw(-raw);        }
  // clang-format on

  // TODO: [missing] No check on the narrowing cast; `wide >> SHIFT` reaches
  //       2^46, so the i32 cast discards real bits. Assert in debug, clamp in
  //       release.
  //
  // TODO: [duplication] This is mul_wide() + narrow() open-coded. fx64.hpp
  //       includes fx.hpp so it cannot be reused directly. Either accept it
  //       and property-test that the two agree on negatives (that test is on
  //       the exit criteria anyway), or move the shift to a shared detail
  //       header. Pick one.
  constexpr fx operator*(fx o) const noexcept {
    // both sides are 65536 too big -> widen to 64 bits then shift off one factor
    const i64 wide = static_cast<i64>(raw) * o.raw;
    return from_raw(static_cast<i32>(wide >> SHIFT));
  }

  // Div-by-zero has no wrapping answer, so it clamps on the numerator's sign.
  //
  // TODO: [BUG] Narrowing cast unchecked. Verified:
  //           from_int(20000)  / from_raw(1)  -> raw 0 (true value 1.31072e9)
  //           from_raw(I32_MIN)/ from_raw(-1) -> raw 0
  //       Assert in debug, clamp to I32_MAX / I32_MIN in release.
  constexpr fx operator/(fx o) const noexcept {
    MOBA_ASSERT(o.raw != 0);
    if (o.raw == 0) return from_raw(raw >= 0 ? I32_MAX : I32_MIN);
    // dividing cancels the scaling entirely, so scale the top first
    const i64 wide = (static_cast<i64>(raw) << SHIFT) / o.raw;
    return from_raw(static_cast<i32>(wide));
  }

  // *= and /= delegate to the binary operators so the guards live in one place.
  // += and -= are plain raw arithmetic because addition needs no rescale.
  // clang-format off
  constexpr fx& operator+=(fx o) noexcept { raw += o.raw;      return *this; }
  constexpr fx& operator-=(fx o) noexcept { raw -= o.raw;      return *this; }
  constexpr fx& operator*=(fx o) noexcept { *this = *this * o; return *this; }
  constexpr fx& operator/=(fx o) noexcept { *this = *this / o; return *this; }
  // clang-format on

  constexpr std::strong_ordering operator<=>(const fx&) const = default;
};

// NOTE: I don't know how to do this below thing
// TODO: [missing] Layout static_asserts. World is memcpy-ed and hashed
//       byte-wise, which rests on properties nothing checks:
//           sizeof == 4, alignof == 4, is_trivially_copyable_v,
//           is_standard_layout_v, has_unique_object_representations_v
//       The last guarantees no padding bits, so equal values always hash
//       equal. Without it, byte-wise hashing can report a desync that never
//       happened. Header, not test. All five already hold on both compilers,
//       so adding them is zero-risk.

// TODO: [missing] from_ratio(i32 num, i32 den), consteval, exact. Frame time
//       is 1/60, decay 3/4. `fx::from_ratio(3, 4)` avoids the long double path
//       below entirely.

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
//       If you keep the float form, pin awkward literals' raw values in a test
//       that runs on both architectures.
consteval fx operator""_fx(long double v) {
  return fx::from_raw(static_cast<i32>(v * fx::SCALE));
}

// TODO: [missing] Surface needed by vec2/angle/shapes: abs, min, max, clamp,
//       lerp, floor, ceil, round, frac, sign. Add as callers appear.
//       lerp(a,b,0)==a and lerp(a,b,1)==b exactly is an exit criterion.

// TODO: [missing] Mixed int arithmetic. `v * 3` does not compile.
//       `fx * i32` is `from_raw(raw * n)`: one multiply, no widen, no shift,
//       no rounding -- cheaper AND exact, unlike going via from_int.
//       Add operator*(i32), operator/(i32), and reversed operator*(i32, fx).
//
//       Do NOT add implicit conversion from int: `x + 1` would mean +1/65536.
//       Same reason to skip heterogeneous comparison (`x > 0` invites `x > 1`
//       meaning raw 1). Use fx::ZERO.

// DECIDED: no operator double() here, ever. to_double(fx) lives in
// <moba/fx/format.hpp> instead, so the include graph keeps floating point out
// of sim TUs with nothing to remember and no macro to get wrong.

// TODO: [decide] A `fixed_point` concept over fx and fx64. abs/min/max/clamp/
//       lerp are the same code twice, and drift between them is the
//       rounding-disagreement bug class. Do it when the second copy is needed,
//       but do not design the free functions in a way that blocks it.

// TODO: [sequencing] Dimensional units on fx (metres vs seconds vs damage).
//       Not the same job as type-safe IDs -- those are in, see
//       <moba/core/strong_id.hpp>. Difference is arithmetic closure: an id has
//       no arithmetic to define; a unit-typed scalar needs an exponent algebra
//       because metres/seconds is a third type.
//       Revisit end of phase 2, when the real unit set is known rather than
//       guessed. Free thing to do now: keep named quantities in signatures --
//       one `fx radius` converts later, four bare fx in a row does not.

} // namespace moba
