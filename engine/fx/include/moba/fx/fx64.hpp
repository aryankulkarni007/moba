#pragma once

// Overflow policy is stated once at the top of <moba/fx/fx.hpp> and applies
// here identically: abort in debug, wrap in release; clamp only where wrapping
// does not exist.
#include <compare>
#include <moba/core/assert.hpp>
#include <moba/fx/fx.hpp>

namespace moba {
// TODO: [missing] Layout static_asserts. Same list as fx.hpp; accumulators
//       live in World and World is memcpy-ed and hashed byte-wise. Verified to
//       already hold on Clang and GCC 16, so adding them is zero-risk.

struct [[nodiscard]] fx64 {
  static constexpr int SHIFT = 32;
  static constexpr i64 SCALE = i64{ 1 } << SHIFT; // 4294967296

  i64 raw = 0;

  static constexpr fx64 from_raw(i64 r) noexcept { return fx64{ r }; }

  static constexpr fx64 from_int(i32 n) noexcept { return fx64{ i64{ n } * SCALE }; }

  static constexpr fx64 widen(fx v) noexcept {
    return from_raw(i64{ v.raw } << (fx64::SHIFT - fx::SHIFT));
  }

  // clang-format off

  constexpr fx64 operator+(fx64 o) const noexcept { return from_raw(raw + o.raw); }
  constexpr fx64 operator-(fx64 o) const noexcept { return from_raw(raw - o.raw); }
  constexpr fx64 operator-()       const noexcept { return from_raw(-raw);        }

  // TODO: [BUG] Dead assert. The two clamping returns fire first, so the
  //       MOBA_ASSERT below can never fail -- debug is silent where it should
  //       abort. Verified: MAX * MAX returns I64_MAX with no diagnostic.
  //       Fix: assert FIRST, then clamp. Order matters for every site here.
  //
  // TODO: [minor] The `>> SHIFT` on a signed i128 is a vendor extension, not
  //       covered by the C++20 arithmetic-shift guarantee. Both compilers do
  //       arithmetic; worth one static_assert pinning it.
  constexpr fx64 operator*(fx64 o) const noexcept {
    const i128 wide = static_cast<i128>(raw) * o.raw;
    const i128 shifted = wide >> SHIFT;
    // saturating addition behaviour
    if (shifted > I64_MAX) return from_raw(I64_MAX);
    if (shifted < I64_MIN) return from_raw(I64_MIN);
    MOBA_ASSERT(shifted >= I64_MIN && shifted <= I64_MAX);
    return from_raw(static_cast<i64>(shifted));

  }

  // Div-by-zero has no wrapping answer, so it clamps on the numerator's sign.
  //
  // TODO: [BUG] The range assert has no release half, so NDEBUG truncates
  //       silently while debug aborts -- the two builds disagree. Verified:
  //           from_raw(I64_MAX) / from_raw(1)  ->  debug: abort
  //                                                NDEBUG: fx64(-4294967296)
  //       Two positive operands, negative result. Add the clamp after the
  //       assert, matching operator*.
  //
  // TODO: [missing] Pin I64_MIN / -1 with a test rather than reasoning.
   constexpr fx64 operator/(fx64 o) const noexcept {
    MOBA_ASSERT(o.raw != 0);
    if (o.raw == 0) return from_raw(raw >= 0 ? I64_MAX : I64_MIN);
    const i128 wide = (static_cast<i128>(raw) << SHIFT) / o.raw;
    MOBA_ASSERT(wide >= I64_MIN && wide <= I64_MAX);
    return from_raw(static_cast<i64>(wide));
  }

  // TODO: [missing] Debug asserts on +, -, unary -, +=, -=. Wrapping in release
  //       is the policy; the missing half is the debug abort. -from_raw(I64_MIN)
  //       returns I64_MIN unchanged, verified.
  //
  // TODO: [missing] Still thin: no *=, /=, operator*(fx), operator*(i32), or
  //       to_int(). operator*(fx) has a phase 2 caller -- damage-over-time
  //       scaling a Q32.32 total by a Q16.16 factor. Define each as
  //       `*this = *this OP o` so guards live in one place.
  //
  // TODO: [minor] +=/-= (fx) exist but there is no binary operator+(fx), so
  //       `a += b` compiles and `a + b` does not.
  constexpr fx64& operator+=(fx64 o)     noexcept { raw += o.raw; return *this;   }
  constexpr fx64& operator-=(fx64 o)     noexcept { raw -= o.raw; return *this;   }

  constexpr fx64& operator+=(fx v)       noexcept { return *this += widen(v);     }
  constexpr fx64& operator-=(fx v)       noexcept { return *this -= widen(v);     }

  constexpr std::strong_ordering operator<=>(const fx64&) const = default;
  // clang-format on
};

// Exact and total: Q16.16 * Q16.16 is Q32.32 with no shift and no loss, and
// the product maxes at 2^62, so it cannot overflow i64. No check needed. This
// is the primitive fx::operator* should reuse.
//
// Same for from_int above: I32_MAX * 2^32 fits i64 and I32_MIN * 2^32 is
// exactly I64_MIN, so it is total over i32 -- unlike fx::from_int.
[[nodiscard]] constexpr fx64 mul_wide(fx a, fx b) noexcept {
  return fx64::from_raw(i64{ a.raw } * b.raw);
}

// Clamping here is correct: narrowing is lossy, there is no wrapping answer.
//
// TODO: [missing] No debug assert, so a clamp that should be a bug report is
//       silent in both builds. Verified: narrow(fx64::from_raw(I64_MAX))
//       returns I32_MAX with no diagnostic.
//
// TODO: [DETERMINISM] `>> 16` floors, matching fx::operator*. But nothing
//       proves fx::operator*, mul_wide+narrow, and fx64::operator* round the
//       same way on negatives. Three paths, three chances to disagree, and a
//       1-ULP disagreement is a desync. Highest-value test in the fx suite.
[[nodiscard]] constexpr fx narrow(fx64 v) noexcept {
  const i64 shifted = v.raw >> (fx64::SHIFT - fx::SHIFT);
  // saturating behaviour
  if (shifted > I32_MAX) return fx::from_raw(I32_MAX);
  if (shifted < I32_MIN) return fx::from_raw(I32_MIN);
  return fx::from_raw(static_cast<i32>(shifted));
}

} // namespace moba
