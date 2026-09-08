#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/shapes.hpp>

// main() comes from tests/doctest_main.cpp.

// covers <moba/fx/shapes.hpp>.
//
// Two kinds of check, because one alone is not enough:
//
//   1. an algebraic reference built from the raws in i128, which shares no
//      code with shapes.hpp. It catches a wrong region or a wrong scale.
//   2. a sampled minimum over the actual segment. Slower and approximate, but
//      genuinely independent of the formula, so it catches a formula that is
//      self-consistently wrong.
//
// Plus the three regressions by value, because all three shipped: a
// point-segment distance measured to the infinite line, a parallel
// segment-segment case off by 4x, and a fourth-power term that overflowed.

namespace {

using namespace moba;

/// Squared distance from p to seg, in Q32.32 raw, computed straight from the
/// fx raws. d^2 = (|ap|^2 |ab|^2 - dot^2) / |ab|^2 as an exact rational,
/// floored. Independent of vec2.hpp and of the cross-product form.
[[nodiscard]] constexpr i128 ref_pt_seg(vec2 p, segment s) noexcept {
  const i128 abx = i128{ s.b.x.raw } - s.a.x.raw;
  const i128 aby = i128{ s.b.y.raw } - s.a.y.raw;
  const i128 apx = i128{ p.x.raw } - s.a.x.raw;
  const i128 apy = i128{ p.y.raw } - s.a.y.raw;

  const i128 den = abx * abx + aby * aby;
  const i128 ap2 = apx * apx + apy * apy;
  if (den == 0) return ap2;

  const i128 num = apx * abx + apy * aby;
  if (num <= 0) return ap2;
  if (num >= den) {
    const i128 bpx = i128{ p.x.raw } - s.b.x.raw;
    const i128 bpy = i128{ p.y.raw } - s.b.y.raw;
    return bpx * bpx + bpy * bpy;
  }
  return (ap2 * den - num * num) / den;
}

[[nodiscard]] constexpr vec2 V(i32 x, i32 y) noexcept {
  return vec2{ fx::from_int(x), fx::from_int(y) };
}
[[nodiscard]] constexpr segment S(i32 ax, i32 ay, i32 bx, i32 by) noexcept {
  return segment{ V(ax, ay), V(bx, by) };
}
[[nodiscard]] constexpr circle C(i32 x, i32 y, i32 r) noexcept {
  return circle{ V(x, y), fx::from_int(r) };
}
[[nodiscard]] constexpr aabb B(i32 x0, i32 y0, i32 x1, i32 y1) noexcept {
  return aabb{ V(x0, y0), V(x1, y1) };
}
[[nodiscard]] constexpr capsule K(segment s, i32 r) noexcept {
  return capsule{ s, fx::from_int(r) };
}

/// The point at parameter k/n along seg, in fx. Used to sample a segment and
/// take the minimum the hard way.
[[nodiscard]] constexpr vec2 at(segment s, i32 k, i32 n) noexcept {
  const fx t = fx::from_raw(static_cast<i32>((i64{ k } << fx::SHIFT) / n));
  return s.a + (s.b - s.a) * t;
}

/// Smallest distance_sq from p to any of n+1 sample points on seg. An upper
/// bound on the true answer, and a tight one for a well-sampled short segment.
[[nodiscard]] constexpr fx64 sampled_pt_seg(vec2 p, segment s, i32 n) noexcept {
  fx64 best = distance_sq(p, s.a);
  for (i32 k = 1; k <= n; ++k) best = min(best, distance_sq(p, at(s, k, n)));
  return best;
}

[[nodiscard]] constexpr fx64 sampled_seg_seg(segment a, segment b, i32 n) noexcept {
  fx64 best = sampled_pt_seg(a.a, b, n);
  for (i32 k = 1; k <= n; ++k) {
    best = min(best, sampled_pt_seg(at(a, k, n), b, n));
  }
  return best;
}

struct tally {
  i64 checked = 0;
  i64 failures = 0;
  constexpr void probe(bool ok) noexcept {
    ++checked;
    if (!ok) ++failures;
  }
};
}  // namespace

TEST_SUITE("fx/shapes") {
  TEST_CASE("shapes: sq is exact where r*r would wrap") {
    CHECK(sq(fx::from_int(3)) == fx64::from_int(9));
    CHECK(sq(fx::from_int(-3)) == fx64::from_int(9));
    CHECK(sq(fx::ZERO) == fx64::ZERO);
    CHECK(sq(fx::ONE) == fx64::ONE);

    // 181 is where an fx-valued square stops fitting. sq keeps going.
    CHECK(sq(fx::from_int(181)) == fx64::from_int(32761));
    CHECK(sq(fx::from_int(182)) == fx64::from_int(33124));
    CHECK(sq(fx::from_int(30000)) == fx64::from_int(900000000));

    // and it is mul_wide, so it agrees with the exact product of the raws
    for (i32 k = 0; k <= 2000; k += 7) {
      const fx v = fx::from_int(k);
      CHECK(i128{ sq(v).raw } == i128{ v.raw } * v.raw);
    }
  }

  TEST_CASE("shapes: contains an aabb point") {
    const aabb box = B(0, 0, 10, 10);
    CHECK(contains(box, V(5, 5)));
    CHECK(contains(box, V(0, 0)));    // corner
    CHECK(contains(box, V(10, 10)));  // opposite corner
    CHECK(contains(box, V(0, 5)));    // edge
    CHECK_FALSE(contains(box, V(11, 5)));
    CHECK_FALSE(contains(box, V(5, -1)));
    CHECK_FALSE(contains(box, V(-1, -1)));

    // a degenerate box is a point and contains exactly that point
    const aabb pt = B(3, 3, 3, 3);
    CHECK(contains(pt, V(3, 3)));
    CHECK_FALSE(contains(pt, V(3, 4)));
  }

  TEST_CASE("shapes: aabb against aabb") {
    const aabb a = B(0, 0, 10, 10);
    CHECK(overlaps(a, B(5, 5, 15, 15)));    // corner overlap
    CHECK(overlaps(a, B(2, 2, 8, 8)));      // fully inside
    CHECK(overlaps(a, B(-5, -5, 15, 15)));  // fully containing
    CHECK(overlaps(a, a));                  // itself
    CHECK_FALSE(overlaps(a, B(11, 0, 20, 10)));
    CHECK_FALSE(overlaps(a, B(0, 11, 10, 20)));

    // touching is a hit, on an edge and on a single corner
    CHECK(overlaps(a, B(10, 0, 20, 10)));
    CHECK(overlaps(a, B(10, 10, 20, 20)));

    // symmetric
    for (i32 dx = -12; dx <= 12; ++dx) {
      const aabb b = B(dx, 0, dx + 10, 10);
      CHECK(overlaps(a, b) == overlaps(b, a));
    }
  }

  TEST_CASE("shapes: circle against circle") {
    CHECK(overlaps(C(0, 0, 1), C(1, 0, 1)));
    CHECK(overlaps(C(0, 0, 5), C(1, 1, 1)));  // nested
    CHECK_FALSE(overlaps(C(0, 0, 1), C(3, 0, 1)));

    // exactly touching, radius sum equal to the distance
    CHECK(overlaps(C(0, 0, 1), C(2, 0, 1)));
    CHECK(overlaps(C(0, 0, 3), C(3, 4, 2)));  // 3-4-5

    // one ULP further apart is a miss
    const circle a{ V(0, 0), fx::from_int(1) };
    const circle b{ vec2{ fx::from_raw(fx::from_int(2).raw + 1), fx::ZERO },
                    fx::from_int(1) };
    CHECK_FALSE(overlaps(a, b));

    // a zero-radius circle is a point probe, which only <= makes usable
    CHECK(overlaps(C(0, 0, 0), C(0, 0, 1)));
    CHECK(overlaps(C(1, 0, 0), C(0, 0, 1)));
    CHECK_FALSE(overlaps(C(2, 0, 0), C(0, 0, 1)));
  }

  TEST_CASE("shapes: point to aabb distance") {
    const aabb box = B(0, 0, 10, 10);
    CHECK(dist_sq_point_aabb(V(5, 5), box) == fx64::ZERO);   // inside
    CHECK(dist_sq_point_aabb(V(0, 0), box) == fx64::ZERO);   // on a corner
    CHECK(dist_sq_point_aabb(V(13, 5), box) == fx64::from_int(9));
    CHECK(dist_sq_point_aabb(V(5, -3), box) == fx64::from_int(9));
    CHECK(dist_sq_point_aabb(V(13, 14), box) == fx64::from_int(25));  // corner

    CHECK(overlaps(C(13, 5, 3), box));
    CHECK_FALSE(overlaps(C(14, 5, 3), box));
    CHECK(overlaps(C(5, 5, 0), box));  // zero-radius probe inside
  }

  TEST_CASE("shapes: point to segment picks the right region") {
    const segment seg = S(0, 0, 10, 0);

    // middle: the perpendicular
    CHECK(dist_sq_point_segment(V(5, 3), seg) == fx64::from_int(9));
    CHECK(dist_sq_point_segment(V(5, -3), seg) == fx64::from_int(9));
    CHECK(dist_sq_point_segment(V(5, 0), seg) == fx64::ZERO);

    // the regression: without the clamp this measured to the infinite line and
    // returned 9 for both of these
    CHECK(dist_sq_point_segment(V(20, 3), seg) == fx64::from_int(109));
    CHECK(dist_sq_point_segment(V(-20, 3), seg) == fx64::from_int(409));
    CHECK(dist_sq_point_segment(V(13, 4), seg) == fx64::from_int(25));

    // endpoints exactly
    CHECK(dist_sq_point_segment(V(0, 0), seg) == fx64::ZERO);
    CHECK(dist_sq_point_segment(V(10, 0), seg) == fx64::ZERO);

    // a zero-length segment is the point, not a division by zero
    const segment degenerate = S(3, 4, 3, 4);
    CHECK(dist_sq_point_segment(V(0, 0), degenerate) == fx64::from_int(25));
    CHECK(dist_sq_point_segment(V(3, 4), degenerate) == fx64::ZERO);
  }

  TEST_CASE("shapes: point to segment against an algebraic reference") {
    // ref_pt_seg works from the fx raws in i128 and shares no code with
    // shapes.hpp or vec2.hpp.
    tally t{};
    const segment segs[] = { S(0, 0, 10, 0),  S(0, 0, 0, 10),  S(0, 0, 7, 7),
                             S(-5, -5, 5, 5), S(-8, 3, 6, -2), S(4, 4, 4, 4) };
    for (const segment &s : segs) {
      for (i32 x = -20; x <= 20; ++x) {
        for (i32 y = -20; y <= 20; ++y) {
          const vec2 p = V(x, y);
          t.probe(i128{ dist_sq_point_segment(p, s).raw } == ref_pt_seg(p, s));
        }
      }
    }
    CHECK(t.checked == 6 * 41 * 41);
    CHECK(t.failures == 0);
  }

  TEST_CASE("shapes: point to segment is the minimum over the segment") {
    // Independent of the formula: walk the segment and take the smallest
    // distance found. The answer may not exceed any sample, and with 256
    // samples on a short segment it may not fall far below the best one.
    tally upper{};
    tally lower{};
    const segment segs[] = { S(0, 0, 10, 0), S(0, 0, 6, 8), S(-4, 2, 9, -3) };
    for (const segment &s : segs) {
      for (i32 x = -12; x <= 12; x += 3) {
        for (i32 y = -12; y <= 12; y += 3) {
          const vec2  p    = V(x, y);
          const fx64  got  = dist_sq_point_segment(p, s);
          const fx64  best = sampled_pt_seg(p, s, 256);
          upper.probe(got.raw <= best.raw);
          lower.probe(best.raw - got.raw <= fx64::from_int(1).raw);
        }
      }
    }
    CHECK(upper.checked == 3 * 9 * 9);
    CHECK(upper.failures == 0);
    CHECK(lower.failures == 0);
  }

  TEST_CASE("shapes: segments intersect") {
    CHECK(segments_intersect(S(0, 0, 10, 0), S(5, -5, 5, 5)));   // crossing
    CHECK(segments_intersect(S(0, 0, 10, 10), S(0, 10, 10, 0)));  // diagonal X
    CHECK_FALSE(segments_intersect(S(0, 0, 10, 0), S(0, 1, 10, 1)));   // parallel
    CHECK_FALSE(segments_intersect(S(0, 0, 10, 0), S(20, 0, 30, 0)));  // collinear apart
    CHECK_FALSE(segments_intersect(S(0, 0, 1, 0), S(5, -5, 5, 5)));    // too short to reach

    // touching counts, whether at an endpoint or along a collinear overlap
    CHECK(segments_intersect(S(0, 0, 10, 0), S(10, 0, 10, 10)));
    CHECK(segments_intersect(S(0, 0, 10, 0), S(5, 0, 5, 10)));   // T junction
    CHECK(segments_intersect(S(0, 0, 10, 0), S(5, 0, 15, 0)));   // collinear overlap
    CHECK(segments_intersect(S(0, 0, 10, 0), S(10, 0, 20, 0)));  // collinear touch

    // symmetric
    const segment probe = S(3, -4, 8, 6);
    for (i32 dx = -15; dx <= 15; ++dx) {
      const segment q = S(dx, 0, dx + 5, 5);
      CHECK(segments_intersect(probe, q) == segments_intersect(q, probe));
    }
  }

  TEST_CASE("shapes: segment to segment distance") {
    // crossing is zero
    CHECK(dist_sq_segment_segment(S(0, 0, 10, 0), S(5, -5, 5, 5)) == fx64::ZERO);

    // the parallel regression. The parametric version pinned s to 0 and
    // returned 401 here and 400 below.
    CHECK(
        dist_sq_segment_segment(S(0, 0, 10, 0), S(20, 1, 30, 1))
        == fx64::from_int(101)
    );
    CHECK(
        dist_sq_segment_segment(S(0, 0, 10, 0), S(20, 0, 30, 0))
        == fx64::from_int(100)
    );

    // ordinary parallel, and the answer is the gap
    CHECK(
        dist_sq_segment_segment(S(0, 0, 10, 0), S(0, 5, 10, 5))
        == fx64::from_int(25)
    );

    // symmetric
    const segment a = S(0, 0, 10, 4);
    for (i32 dx = -20; dx <= 20; dx += 2) {
      const segment b = S(dx, 8, dx + 6, 12);
      CHECK(dist_sq_segment_segment(a, b) == dist_sq_segment_segment(b, a));
    }
  }

  TEST_CASE("shapes: segment to segment is the minimum over both") {
    tally upper{};
    tally lower{};
    const segment as[] = { S(0, 0, 8, 0), S(0, 0, 5, 6), S(-4, 3, 7, -2) };
    for (const segment &a : as) {
      for (i32 dx = -14; dx <= 14; dx += 4) {
        for (i32 dy = -14; dy <= 14; dy += 4) {
          const segment b   = S(dx, dy, dx + 6, dy + 3);
          const fx64    got = dist_sq_segment_segment(a, b);
          const fx64    best = sampled_seg_seg(a, b, 32);
          upper.probe(got.raw <= best.raw);
          lower.probe(best.raw - got.raw <= fx64::from_int(2).raw);
        }
      }
    }
    CHECK(upper.checked == 3 * 8 * 8);
    CHECK(upper.failures == 0);
    CHECK(lower.failures == 0);
  }

  TEST_CASE("shapes: a capsule is not an infinite line") {
    // The gameplay symptom of the missing clamp: this capsule ends at x=10 and
    // used to report a hit against a body at x=384.
    const capsule sword = K(S(0, 0, 10, 0), 1);
    CHECK(overlaps(C(8, 0, 1), sword));
    CHECK(overlaps(C(10, 0, 1), sword));
    CHECK(overlaps(C(12, 0, 1), sword));  // exactly touching the end cap
    CHECK_FALSE(overlaps(C(13, 0, 1), sword));
    CHECK_FALSE(overlaps(C(24, 0, 1), sword));
    CHECK_FALSE(overlaps(C(96, 0, 1), sword));
    CHECK_FALSE(overlaps(C(384, 0, 1), sword));

    // and it is still a capsule sideways on
    CHECK(overlaps(C(5, 2, 1), sword));
    CHECK_FALSE(overlaps(C(5, 3, 1), sword));
  }

  TEST_CASE("shapes: capsule against capsule") {
    const capsule a = K(S(0, 0, 10, 0), 1);
    CHECK(overlaps(a, K(S(5, -5, 5, 5), 1)));    // crossing
    CHECK(overlaps(a, K(S(0, 2, 10, 2), 1)));    // exactly touching
    CHECK_FALSE(overlaps(a, K(S(0, 3, 10, 3), 1)));
    CHECK_FALSE(overlaps(a, K(S(20, 0, 30, 0), 1)));  // collinear and clear

    // segments long enough that the parametric denom overflowed fx64. The
    // fourth-power term is gone, so these are ordinary cases now.
    const capsule long_a = K(S(0, 0, 300, 0), 1);
    const capsule long_b = K(S(0, 100, 0, 300), 1);
    CHECK_FALSE(overlaps(long_a, long_b));
    CHECK(overlaps(long_a, K(S(150, -50, 150, 50), 1)));
    CHECK(overlaps(K(S(0, 0, 1000, 0), 1), K(S(500, -5, 500, 5), 1)));
  }

  TEST_CASE("shapes: touching is a hit in every predicate") {
    // One decision, stated at the top of shapes.hpp, checked once per
    // predicate. A predicate that drifted to < would fail exactly here.
    CHECK(overlaps(C(0, 0, 1), C(2, 0, 1)));                 // circle/circle
    CHECK(overlaps(C(13, 5, 3), B(0, 0, 10, 10)));           // circle/aabb
    CHECK(overlaps(B(0, 0, 10, 10), B(10, 10, 20, 20)));     // aabb/aabb
    CHECK(contains(B(0, 0, 10, 10), V(10, 10)));             // aabb/point
    CHECK(overlaps(C(12, 0, 1), K(S(0, 0, 10, 0), 1)));      // circle/capsule
    CHECK(overlaps(K(S(0, 0, 10, 0), 1), K(S(0, 2, 10, 2), 1)));  // capsule/capsule

    // and one ULP beyond each is a miss, so <= is doing real work
    CHECK_FALSE(overlaps(
        C(0, 0, 1),
        circle{ vec2{ fx::from_raw(fx::from_int(2).raw + 1), fx::ZERO },
                fx::from_int(1) }
    ));
    CHECK_FALSE(contains(
        B(0, 0, 10, 10),
        vec2{ fx::from_raw(fx::from_int(10).raw + 1), fx::from_int(10) }
    ));
  }

  TEST_CASE("shapes: the divisionless comparison agrees with the divided one") {
    // Every predicate compares with detail::at_most, which is
    //     floor(n/m) <= k   <=>   n < (k+1)*m
    // rearranged rather than a different question. arm64 has no 128-bit
    // divide, so this turns a __udivti3 call into a multiply.
    //
    // The rearrangement is exact, and this is what says so. A drift to the
    // simpler n <= k*m would compare the UNFLOORED value and fail here at the
    // boundary, which is the whole risk of having two forms.
    tally t{};
    const segment segs[] = { S(0, 0, 10, 0), S(0, 0, 7, 7), S(-8, 3, 6, -2),
                             S(4, 4, 4, 4) };
    for (const segment &sg : segs) {
      for (i32 x = -14; x <= 14; ++x) {
        for (i32 y = -14; y <= 14; ++y) {
          const vec2            p = V(x, y);
          const detail::sq_dist d = detail::pt_seg(p, sg);
          const fx64            v = detail::floor_of(d);
          CHECK(v == dist_sq_point_segment(p, sg));

          // sweep k across the answer, so the boundary is hit exactly
          for (i64 off = -2; off <= 2; ++off) {
            const fx64 k = fx64::from_raw(v.raw + off);
            t.probe(detail::at_most(d, k) == (v <= k));
          }
        }
      }
    }
    CHECK(t.checked == 4 * 29 * 29 * 5);
    CHECK(t.failures == 0);
  }

  TEST_CASE("shapes: predicates agree with distance compared by hand") {
    // The user-visible half of the same guarantee: a caller who writes the
    // comparison out gets the same answer the predicate gives.
    tally cap{};
    tally seg{};
    const capsule a = K(S(0, 0, 10, 0), 2);
    for (i32 x = -6; x <= 20; ++x) {
      for (i32 y = -8; y <= 8; ++y) {
        for (i32 r = 0; r <= 3; ++r) {
          const circle c = C(x, y, r);
          cap.probe(
              overlaps(c, a)
              == (dist_sq_point_segment(c.c, a.seg) <= sq(c.r + a.r))
          );
          const capsule b = K(S(x, y, x + 4, y + 3), r);
          seg.probe(
              overlaps(a, b)
              == (dist_sq_segment_segment(a.seg, b.seg) <= sq(a.r + b.r))
          );
        }
      }
    }
    CHECK(cap.checked == 27 * 17 * 4);
    CHECK(cap.failures == 0);
    CHECK(seg.failures == 0);
  }

  TEST_CASE("shapes: constexpr") {
    constexpr aabb    box  = B(0, 0, 10, 10);
    constexpr circle  ball = C(13, 5, 3);
    constexpr segment seg  = S(0, 0, 10, 0);

    static_assert(sq(fx::from_int(3)).raw == fx64::from_int(9).raw);
    static_assert(contains(box, V(5, 5)));
    static_assert(!contains(box, V(11, 5)));
    static_assert(overlaps(box, B(5, 5, 15, 15)));
    static_assert(overlaps(ball, box));
    static_assert(dist_sq_point_aabb(V(13, 5), box).raw == fx64::from_int(9).raw);
    static_assert(dist_sq_point_segment(V(20, 3), seg).raw
                  == fx64::from_int(109).raw);
    static_assert(segments_intersect(seg, S(5, -5, 5, 5)));
    static_assert(dist_sq_segment_segment(seg, S(20, 1, 30, 1)).raw
                  == fx64::from_int(101).raw);
    static_assert(overlaps(C(0, 0, 1), C(2, 0, 1)));
    CHECK(true);
  }

  TEST_CASE("shapes: byte representation") {
    // These go in the rollback snapshot alongside vec2, so the same rule
    // applies: the object is its bytes, with no padding to hold junk.
    static_assert(sizeof(circle) == 12);
    static_assert(sizeof(segment) == 16);
    static_assert(sizeof(aabb) == 16);
    static_assert(sizeof(capsule) == 20);

    static_assert(std::is_trivially_copyable_v<circle>);
    static_assert(std::is_trivially_copyable_v<segment>);
    static_assert(std::is_trivially_copyable_v<aabb>);
    static_assert(std::is_trivially_copyable_v<capsule>);

    static_assert(std::has_unique_object_representations_v<circle>);
    static_assert(std::has_unique_object_representations_v<segment>);
    static_assert(std::has_unique_object_representations_v<aabb>);
    static_assert(std::has_unique_object_representations_v<capsule>);

    static_assert(std::is_standard_layout_v<circle>);
    static_assert(std::is_standard_layout_v<capsule>);
    CHECK(sizeof(capsule) == 20);
  }
}
