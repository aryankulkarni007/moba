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

  static const vec2 ZERO, ONE;
};

inline constexpr vec2 vec2::ZERO = vec2{ 0_fx, 0_fx };
inline constexpr vec2 vec2::ONE  = vec2{ 1_fx, 1_fx };

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

/// exact floor of the length
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

/// DECIDED: a zero vector normalises to ZERO. Asserts in debug, returns
/// vec2::ZERO in release -- the same degenerate policy as shapes.hpp, where a
/// zero-length segment is the point itself.
///
/// ZERO rather than an invented direction such as (1, 0), because normalise(0)
/// has no correct answer and the question is only which wrong answer is
/// easiest to SEE. A zero propagates visibly: a character that does not move.
/// A silent (1, 0) is a character drifting east for a reason nobody can find.
/// Previously it returned (I32_MAX, I32_MAX) -- what fx division by zero
/// happens to do -- which was an accident rather than a decision.
///
/// The guard covers every case: length() is at least one raw unit for any
/// non-zero v, since length_sq's raw is then at least 1 and isqrt(1) is 1. So
/// the only way to divide by zero here is to pass exactly ZERO.
///
/// ACCURACY. length() floors, so the relative error is about EPSILON/|v|:
/// negligible at 1 unit and upward, 1.5% for a vector 0.001 units long. Two
/// short vectors that differ in direction can normalise to the same pair.
/// Where a unit vector is wanted from a known heading, from_angle(a) is exact
/// by construction and this is not -- prefer it once it exists.
constexpr vec2 normalise(vec2 v) noexcept {
  MOBA_ASSERT(v != vec2::ZERO);
  if (v == vec2::ZERO) return vec2::ZERO;
  return v / length(v);
}

/// DECIDED: anticlockwise -- a quarter turn the positive way, (x, y) -> (-y, x).
///
/// The reason is `cross`, not taste. `cross(a, b)` is already positive when b
/// lies anticlockwise of a, and the ACW perpendicular gives
/// `cross(v, perp(v)) == length_sq(v)`, which is positive for every non-zero
/// v. The clockwise version gives -length_sq(v) and would leave two opposite
/// notions of "the positive direction of rotation" in one library, which is
/// the kind of disagreement that shows up as a normal pointing into a wall.
/// Pinned by that identity in test_vec2.cpp rather than by a worked example.
constexpr vec2 perp(vec2 v) noexcept { return vec2{ -v.y, v.x }; }

/// The commuted form fx and fx64 both provide. Delegates, so `2_fx * v` and
/// `v * 2_fx` cannot drift apart.
[[nodiscard]] constexpr vec2 operator*(fx s, vec2 v) noexcept { return v * s; }

static_assert(sizeof(vec2) == 8);
static_assert(alignof(vec2) == 4);
static_assert(std::is_trivially_copyable_v<vec2>);
static_assert(std::is_standard_layout_v<vec2>);
static_assert(std::has_unique_object_representations_v<vec2>);
static_assert(!std::is_aggregate_v<vec2>);

// TODO: [missing] rotate/from_angle/to_angle. UNBLOCKED -- angle.hpp is
//       written and tested, and these are its first real callers.
//         rotate(v, a)      v.x*cos - v.y*sin, v.x*sin + v.y*cos
//         from_angle(a)     { cos(a), sin(a) } -- an EXACT unit vector, which
//                           normalise() cannot give for short inputs; see the
//                           note on normalise below
//         to_angle(v)       atan2(v.y, v.x)
//       Not `v * a`: rotation is not a scale, and there is no meaningful
//       product of a vector and an angle.

}  // namespace moba

// moba/fx/vec2.hpp -- 2D fixed-point vector.
//
// struct vec2 { fx x, y; }  -- no invariants.
//
// Surface:
//   + - unary- += -= ==
//   operator*(fx) operator/(fx)
//   dot(a,b) -> fx64            exact via mul_wide; no per-term narrowing
//   cross(a,b) -> fx64          signed area; sign gives which side of a line
//   length_sq(v) -> fx64        written as dot(v, v), so they cannot diverge
//   distance_sq(a,b) -> fx64    written as length_sq(a - b)
//   length(v) -> fx             asserts the root fits i32
//   normalise(v) -> vec2        see the accuracy note at the definition
//   perp(v) -> vec2             ACW; cross(v, perp(v)) == length_sq(v)
//   operator*(fx, vec2)         the commuted scale
//
// DECIDED: dot, cross, length_sq and distance_sq all return fx64. fx's usable
// integer range is about +/-32767, so a product of two coordinates leaves it
// at 181 units apart and two points 300 apart give 90000. Returning fx made
// every range check wrong past ~181 units, and dot(v,v) disagreed with
// length_sq(v) at 200. Correctness, not optimisation, and it sets the
// signature of every range comparison in combat code.
//
// Notes:
//   - Prefer squared comparisons. `distance_sq(a,b) <= mul_wide(r, r)` needs
//     no root, and hitbox tests run per pair per tick. mul_wide rather than
//     r*r: both sides must be fx64, and r*r overflows past 181 units anyway.
//   - shapes.hpp is built entirely on these four and takes no square root.
//
// The zero-vector, layout-assert, perp and rotate TODOs that used to live here
// are gone: two are done and two moved to the definitions they are about, so
// that they are read by anyone editing the code rather than only by anyone
// reading the foot of the file.
//
// Nothing open in this file but rotate/from_angle/to_angle, which have their
// own note beside the layout static_asserts.
