#pragma once

#include <moba/fx/fx.hpp>
#include <moba/fx/fx64.hpp>
#include <moba/fx/isqrt.hpp>

namespace moba {
struct [[nodiscard]] vec2 {
  fx x, y;

  explicit constexpr vec2(fx px, fx py) noexcept : x(px), y(py) {}
  constexpr vec2() noexcept = default;

  constexpr bool operator==(const vec2 &) const = default;

  // clang-format off
  constexpr vec2 operator+(vec2 o) const noexcept { return vec2{ x + o.x, y + o.y }; }
  constexpr vec2 operator-(vec2 o) const noexcept { return vec2{ x - o.x, y - o.y }; }

  constexpr vec2 operator*(fx o) const noexcept   { return vec2{ x * o, y * o }; }
  constexpr vec2 operator/(fx o) const noexcept   { return vec2{ x / o, y / o }; }

  constexpr vec2& operator+=(vec2 o)     noexcept { *this = *this + o; return *this; }
  constexpr vec2& operator-=(vec2 o)     noexcept { *this = *this - o; return *this; }
  // clang-format on
  constexpr vec2 operator-() const noexcept { return vec2{ -x, -y }; }

  static const vec2 ZERO;
};

inline constexpr vec2 vec2::ZERO = vec2{ 0_fx, 0_fx };

constexpr fx64 dot(vec2 a, vec2 b) noexcept {
  return mul_wide(a.x, b.x) + mul_wide(a.y, b.y);
}

/// signed area of the parallelogram the two vectors span; the sign says which
/// side of a line a point falls on.
constexpr fx64 cross(vec2 a, vec2 b) noexcept {
  return mul_wide(a.x, b.y) - mul_wide(a.y, b.x);
}

/// written as dot(v, v) rather than spelled out again, so the identity holds
/// by construction instead of by inspection.
constexpr fx64 length_sq(vec2 v) noexcept { return dot(v, v); }

/// squared distance, which is (a - b) . (a - b), not a.a - b.b. the earlier
/// form returned 8 for a distance of 2, went negative when b was the further
/// point, and was not symmetric.
///
/// Subtracting first means the difference has to be representable as an fx,
/// and fx::operator- asserts when it is not. That is the same limit as storing
/// the coordinates at all, since both operands are already fx. Squaring
/// afterwards is exact.
///
/// The three asserts this replaces were all wrong: the first two checked the
/// wrong quantity, and the third could never fire, since (x1 - x2) + (y1 - y2)
/// is evaluated in i64 and wraps before the widening conversion to i128.
constexpr fx64 distance_sq(vec2 a, vec2 b) noexcept { return length_sq(a - b); }

/// Exact floor of the length.
///
/// length_sq is Q32.32 and a square root halves the scale, so isqrt hands back
/// a Q16.16 raw with no shift at all. See isqrt.hpp.
///
/// The assert is not decoration. isqrt returns u32 and length_sq reaches 2^63,
/// so the root reaches about 3.03e9, which does not fit i32; length of
/// (30000, 30000) came back as raw -1514510296 without it. sqrt(fx) needs no
/// such check because its input tops out at I32_MAX << 16 = 2^47, but
/// length_sq is a sum of two full-width products and reaches 2^63, so that
/// bound does not carry over. A length above fx::MAX cannot be represented and
/// aborts here rather than wrapping to a negative distance.
constexpr fx length(vec2 v) noexcept {
  // a sum of squares, so the raw is non-negative and the u64 cast only changes
  // how the bits are read, not which value they hold
  const u32 root = isqrt(static_cast<u64>(length_sq(v).raw));
  MOBA_ASSERT(root <= static_cast<u32>(I32_MAX));
  return fx::from_raw(static_cast<i32>(root));
}

constexpr vec2 normalise(vec2 v) noexcept {
  MOBA_ASSERT(v != vec2::ZERO);
  return v / length(v);
}

// should this default to the cw or the acw perpendicular?
constexpr vec2 perp(vec2 v) noexcept { return vec2{ v.y, -v.x }; }

static_assert(sizeof(vec2) == 8);
static_assert(alignof(vec2) == 4);
static_assert(std::is_trivially_copyable_v<vec2>);
static_assert(std::is_standard_layout_v<vec2>);
static_assert(std::has_unique_object_representations_v<vec2>);
static_assert(!std::is_aggregate_v<vec2>);

// TODO: (waiting for angle.hpp impl)
// constexpr vec2 rotate(vec2 v, angle a) noexcept { return v * a; }

}  // namespace moba

// moba/fx/vec2.hpp -- 2D fixed-point vector. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Needs fx (works) and angle (not written).
//
// struct vec2 { fx x, y; }  -- POD aggregate, no invariants.
//
// Surface:
//   + - unary- += -=
//   operator*(fx) operator/(fx)
//   dot(a,b) -> fx64
//   cross(a,b) -> fx64          scalar z; sign gives which side of a line
//   length_sq(v) -> fx64        see the decide below
//   length(v) -> fx             needs isqrt
//   distance_sq(a,b) -> fx64
//   normalise(v) -> vec2
//   rotate(v, angle), perp(v), from_angle(angle, fx len), to_angle(v)
//
// TODO: [decide] length_sq must return fx64, not fx. fx's usable integer range
//       is about +/-32767, so two units 300 apart give 90000 and overflow.
//       Returning fx makes every range check wrong beyond ~181 units. This is
//       correctness, not optimisation, and it fixes the signature of every
//       range comparison in combat code.
//
// Notes:
//   - Prefer squared comparisons: `distance_sq(a,b) < mul_wide(r, r)` avoids
//   isqrt, and
//     hitbox tests run per-pair per-tick.
//   - normalise() of a zero vector has no correct answer. Pick one, document
//     it, {0,0} or assert. It must not be "undefined" -- both machines need
//     the same result.
//   - Accumulate in fx64 inside dot/length_sq and narrow once at the end.
//     Narrowing per-term loses precision in an operand-order-dependent way.
