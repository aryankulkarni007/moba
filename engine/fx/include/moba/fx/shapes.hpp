#pragma once

// moba/fx/shapes.hpp -- collision primitives.
//
// TOUCHING IS A HIT. Every predicate uses <=, so exactly-touching circles
// collide, aabbs sharing an edge collide, and a zero-radius circle is a usable
// point probe. One character, and it has to be the same character everywhere:
// a hit that lands in one predicate and misses in another is a bug players
// find before you do.
//
// DEGENERATES. assert in debug, defined answer in release, as everywhere else:
//   zero-length segment  the point itself; an early return, not a special case
//   zero-radius circle   works under <=, no handling needed
//   aabb with min > max  asserts. It is a construction bug, and left alone it
//                        is inconsistent: contains() reports no and
//                        overlaps(aabb, aabb) reports yes for the same region.
//
// NO SQUARE ROOTS. Everything here is a squared distance compared against a
// squared radius, which is why the distances return fx64: fx tops out near
// 32767 and a squared distance passes that at 181 units. raycast would need a
// root and is deliberately absent; see the note at the foot of the file.
//
// NO DIVISIONS EITHER, in the predicates. Only point-to-segment produces a
// ratio, and detail::sq_dist carries it undivided so a comparison against a
// squared radius stays exact and needs no divide; see detail::at_most. arm64
// has no 128-bit divide instruction, so each one avoided is a call into
// __udivti3 rather than an instruction.
//
// The value functions still divide, because returning a ratio as a number
// requires it. Reach for a predicate when a yes/no is all you need.

#include <moba/core/assert.hpp>
#include <moba/core/types.hpp>
#include <moba/fx/fx.hpp>
#include <moba/fx/fx64.hpp>
#include <moba/fx/vec2.hpp>

namespace moba {
struct [[nodiscard]] circle {
  vec2 c;
  fx   r;
};

struct [[nodiscard]] segment {
  vec2 a, b;
};

struct [[nodiscard]] aabb {
  vec2 min, max;
};

struct [[nodiscard]] capsule {
  segment seg;
  fx      r;
};

/// r squared, as the fx64 every distance here is measured in. mul_wide is
/// exact: Q16.16 * Q16.16 is Q32.32 with no shift. `r * r` would wrap past 181.
[[nodiscard]] constexpr fx64 sq(fx v) noexcept { return mul_wide(v, v); }

/// asserted, not enforced. an inverted box has no consistent reading
[[nodiscard]] constexpr bool well_formed(aabb b) noexcept {
  return b.min.x <= b.max.x && b.min.y <= b.max.y;
}

/// p is on seg's line already; this only checks it lies within the span.
[[nodiscard]] constexpr bool within_bounds(vec2 p, segment s) noexcept {
  return min(s.a.x, s.b.x) <= p.x && p.x <= max(s.a.x, s.b.x)
         && min(s.a.y, s.b.y) <= p.y && p.y <= max(s.a.y, s.b.y);
}

/* DISTANCES */

/// Closest point on the box is the centre clamped per axis, which is the box
/// itself when p is inside, giving zero.
[[nodiscard]] constexpr fx64 dist_sq_point_aabb(vec2 p, aabb box) noexcept {
  MOBA_ASSERT(well_formed(box));
  const fx dx = p.x - clamp(p.x, box.min.x, box.max.x);
  const fx dy = p.y - clamp(p.y, box.min.y, box.max.y);
  return sq(dx) + sq(dy);
}

namespace detail {
/// a squared distance left as an exact rational num/den, both Q32.32 raw
/// terms, den > 0. Not dividing is the point: arm64 has no 128-bit divide, so
/// every division here is a call into __udivti3, and a comparison against a
/// squared radius does not need one.
///
/// endpoint answers are exact already and arrive with den = 1.
struct sq_dist {
  i128 num;
  i128 den;
};

/// floor(num/den): the value the public distance functions return, and the one
/// place a division happens. Both operands are non-negative, so truncation is
/// already the floor this library rounds by.
[[nodiscard]] constexpr fx64 floor_of(sq_dist d) noexcept {
  MOBA_ASSERT(d.den > 0);
  const i128 q = d.num / d.den;
  MOBA_ASSERT(fits_i64(q));
  return fx64::from_raw(static_cast<i64>(q));
}

/// floor_of(d) <= k, without dividing:
///
///     floor(n/m) <= k   <=>   n < (k+1)*m      for m > 0
///
/// Exact, so it cannot disagree with floor_of(d) <= k at the boundary. The
/// simpler-looking n <= k*m would disagree: that compares the UNFLOORED value,
/// which is a different question.
///
/// Ranges: n is a squared cross product, at most 2^126. k is a squared radius
/// raw, at most 2^62, and m a squared length raw, at most 2^63, so (k+1)*m is
/// at most 2^125. Both sides fit i128.
[[nodiscard]] constexpr bool at_most(sq_dist d, fx64 k) noexcept {
  MOBA_ASSERT(d.den > 0);
  return d.num < (i128{ k.raw } + 1) * d.den;
}

[[nodiscard]] constexpr sq_dist exact(fx64 v) noexcept {
  return sq_dist{ .num = i128{ v.raw }, .den = 1 };
}

/// Three regions, picked by sign alone. Only the middle one is a ratio.
///
/// The projection parameter t = dot(ap,ab)/|ab|^2 is never formed. Clamping it
/// to [0,1] only needs the two comparisons below, and materialising it would
/// cost a division and two roundings.
///
/// The middle region is cross(ap,ab)^2 / |ab|^2, returned undivided. Preferred
/// over |ap|^2 - dot(ap,ab)^2/|ab|^2, which is algebraically equal by Lagrange
/// but subtracts two near-equal numbers exactly when p is near the line.
///
/// Scaling: cross and |ab|^2 are both Q32.32, so C*C/D is (C^2*2^64)/(D*2^32),
/// which is Q32.32 again with no shift.
[[nodiscard]] constexpr sq_dist pt_seg(vec2 p, segment seg) noexcept {
  const vec2 ab = seg.b - seg.a;
  const vec2 ap = p - seg.a;

  const fx64 den = length_sq(ab);
  if (den == fx64::ZERO) return exact(distance_sq(p, seg.a));  // degenerate

  const fx64 num = dot(ap, ab);
  if (num <= fx64::ZERO) return exact(distance_sq(p, seg.a));  // before a
  if (num >= den) return exact(distance_sq(p, seg.b));         // past b

  const i128 c = i128{ cross(ap, ab).raw };  // |c| <= 2^63, so c*c fits i128
  return sq_dist{ .num = c * c, .den = i128{ den.raw } };
}
}  // namespace detail

/// The value, which costs the division. Callers wanting only a yes/no should
/// use a predicate below; those compare without dividing.
[[nodiscard]] constexpr fx64
dist_sq_point_segment(vec2 p, segment seg) noexcept {
  return detail::floor_of(detail::pt_seg(p, seg));
}

/// straddle test on four cross products. exact, no division
[[nodiscard]] constexpr bool segments_intersect(segment s, segment t) noexcept {
  const vec2 sd = s.b - s.a;
  const vec2 td = t.b - t.a;

  const i32 d1 = sign(cross(td, s.a - t.a));
  const i32 d2 = sign(cross(td, s.b - t.a));
  const i32 d3 = sign(cross(sd, t.a - s.a));
  const i32 d4 = sign(cross(sd, t.b - s.a));

  // each segment has its endpoints strictly either side of the other's line
  if (d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0 && d1 != d2 && d3 != d4) {
    return true;
  }

  // collinear or an endpoint landing exactly on the other segment
  if (d1 == 0 && within_bounds(s.a, t)) return true;
  if (d2 == 0 && within_bounds(s.b, t)) return true;
  if (d3 == 0 && within_bounds(t.a, s)) return true;
  if (d4 == 0 && within_bounds(t.b, s)) return true;
  return false;
}

/// In 2D the minimum between two non-intersecting segments is always attained
/// at an endpoint of one of them, so no parametric solve is needed. That is
/// what removes the |u|^2 * |v|^2 term the 3D algorithm needs: it is a fourth
/// power of length and overflows fx64 above about 215 units.
///
/// (The endpoint reduction is 2D only. In 3D two skew segments can be closest
/// at interior points of both, which is why the parametric version exists.)
///
/// floor is monotonic, so the min over the four floored answers is the floor
/// of the true minimum; these four divisions are not an approximation.
[[nodiscard]] constexpr fx64
dist_sq_segment_segment(segment s, segment t) noexcept {
  if (segments_intersect(s, t)) return fx64::ZERO;
  return min(
      min(dist_sq_point_segment(s.a, t), dist_sq_point_segment(s.b, t)),
      min(dist_sq_point_segment(t.a, s), dist_sq_point_segment(t.b, s))
  );
}

/* PREDICATES -- all exact, none of them divide.
 *
 * The aabb and circle ones never had a ratio to begin with. The two capsule
 * ones go through detail::at_most, which is floor_of(d) <= k rearranged rather
 * than a different comparison, so a caller who writes the distance comparison
 * out by hand gets the same answer. Held there by "shapes: the divisionless
 * comparison agrees with the divided one" and "shapes: predicates agree with
 * distance compared by hand". */

[[nodiscard]] constexpr bool overlaps(circle a, circle b) noexcept {
  return distance_sq(a.c, b.c) <= sq(a.r + b.r);
}

[[nodiscard]] constexpr bool overlaps(circle a, aabb b) noexcept {
  return dist_sq_point_aabb(a.c, b) <= sq(a.r);
}

[[nodiscard]] constexpr bool overlaps(circle a, capsule b) noexcept {
  return detail::at_most(detail::pt_seg(a.c, b.seg), sq(a.r + b.r));
}

/// min(d1..d4) <= k is d1 <= k || ... || d4 <= k, so the four distances never
/// have to be compared against each other. That matters: comparing two
/// rationals needs a cross multiply, and num * den would reach 2^189.
///
/// It short-circuits too, so a hit usually costs one endpoint test.
[[nodiscard]] constexpr bool overlaps(capsule a, capsule b) noexcept {
  if (segments_intersect(a.seg, b.seg)) return true;
  const fx64 k = sq(a.r + b.r);
  return detail::at_most(detail::pt_seg(a.seg.a, b.seg), k)
         || detail::at_most(detail::pt_seg(a.seg.b, b.seg), k)
         || detail::at_most(detail::pt_seg(b.seg.a, a.seg), k)
         || detail::at_most(detail::pt_seg(b.seg.b, a.seg), k);
}

/// interval overlap per axis. no arithmetic at all, so nothing to round or
/// overflow. `>` for the separation test makes shared edges an overlap.
[[nodiscard]] constexpr bool overlaps(aabb a, aabb b) noexcept {
  MOBA_ASSERT(well_formed(a) && well_formed(b));
  const bool separated_x = a.min.x > b.max.x || b.min.x > a.max.x;
  const bool separated_y = a.min.y > b.max.y || b.min.y > a.max.y;
  return !(separated_x || separated_y);
}

[[nodiscard]] constexpr bool contains(aabb box, vec2 p) noexcept {
  MOBA_ASSERT(well_formed(box));
  return box.min.x <= p.x && p.x <= box.max.x && box.min.y <= p.y
         && p.y <= box.max.y;
}

/// The commuted spellings. They DELEGATE rather than reimplement, and that is
/// the whole point: two orders computing the same thing separately could
/// disagree, and a hit that lands in one and misses in the other is exactly
/// what the touching-is-a-hit rule at the top of this file exists to prevent.
[[nodiscard]] constexpr bool overlaps(aabb a, circle b) noexcept {
  return overlaps(b, a);
}
[[nodiscard]] constexpr bool overlaps(capsule a, circle b) noexcept {
  return overlaps(b, a);
}

/// Completes the point-probe set alongside contains(aabb, vec2). A point is a
/// zero-radius circle under `<=`, as the header note says, so this needs no
/// special case -- and delegating means the touching rule has one definition
/// rather than two that could drift apart.
[[nodiscard]] constexpr bool contains(circle c, vec2 p) noexcept {
  return overlaps(c, circle{ p, 0_fx });
}
}  // namespace moba

// TODO: [decide] raycast(segment, circle) is the only thing here that would
//       need a square root, and it returns an optional, which means either
//       <optional> in a sim header or core/result.hpp, still deferred.
//       Before writing it: a projectile moving over one tick IS a capsule, so
//       swept collision is overlaps(capsule, capsule), which exists, is exact,
//       and fixes tunnelling. Check whether a ray is wanted at all.
//
// TODO: [sequencing] sq(fx) arguably belongs beside mul_wide in fx64.hpp
//       rather than here. Move it when a second file wants it.
