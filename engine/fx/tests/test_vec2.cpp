#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/vec2.hpp>

// main() comes from tests/doctest_main.cpp.

// covers <moba/fx/vec2.hpp>. Until this file existed nothing included the
// header, so nothing compiled it; that is how distance_sq shipped computing
// a.a - b.b and length() shipped returning negative lengths.
//
// Three groups:
//   1. the algebra each function claims (symmetry, antisymmetry, identities)
//   2. sweeps against an independent i128 reference computed from the raws
//   3. the two specific regressions above, by value, so they cannot come back
//
// Coordinates stay inside +/-2000 units throughout. The functions assert on
// overflow in debug, and the point of a sweep is to exercise the arithmetic,
// not the abort path.

namespace {

using namespace moba;

/// dot and cross from the raws in i128: exact, and independent of anything in
/// vec2.hpp. Two Q16.16 raws multiply to Q32.32, which is what fx64 stores, so
/// the products compare directly against the raw with no scaling.
[[nodiscard]] constexpr i128 ref_dot(vec2 a, vec2 b) noexcept {
  return (i128{ a.x.raw } * b.x.raw) + (i128{ a.y.raw } * b.y.raw);
}
[[nodiscard]] constexpr i128 ref_cross(vec2 a, vec2 b) noexcept {
  return (i128{ a.x.raw } * b.y.raw) - (i128{ a.y.raw } * b.x.raw);
}
[[nodiscard]] constexpr i128 ref_distance_sq(vec2 a, vec2 b) noexcept {
  const i128 dx = i128{ a.x.raw } - b.x.raw;
  const i128 dy = i128{ a.y.raw } - b.y.raw;
  return (dx * dx) + (dy * dy);
}

[[nodiscard]] constexpr vec2 whole(i32 x, i32 y) noexcept {
  return vec2{ fx::from_int(x), fx::from_int(y) };
}

/// Counts over a sweep rather than one CHECK per pair: a CHECK per input is
/// slow at this many iterations and unreadable when it does fail.
struct tally {
  i64 checked  = 0;
  i64 failures = 0;

  constexpr void probe(bool ok) noexcept {
    ++checked;
    if (!ok) ++failures;
  }
};
}  // namespace

TEST_SUITE("fx/vec2") {
  TEST_CASE("vec2: construction and equality") {
    // The default constructor exists so vec2 can sit in an array or a snapshot
    // struct. fx's own default gives raw 0, so a default vec2 is the origin.
    constexpr vec2 origin{};
    static_assert(origin.x.raw == 0 && origin.y.raw == 0);

    const vec2 a{ fx::from_int(3), fx::from_int(4) };
    const vec2 b{ fx::from_int(3), fx::from_int(4) };
    const vec2 c{ fx::from_int(3), fx::from_int(5) };

    CHECK(a == b);
    CHECK(a != c);
    CHECK_FALSE(a == c);

    // memberwise through fx's exact integer comparison,
    // so one ULP apart is not equal
    const vec2 d{ fx::from_raw(a.x.raw + 1), a.y };
    CHECK(a != d);

    // the constructor is explicit, so a bare pair of fx does not silently
    // become a vector; this is the spelling that works
    CHECK(vec2{ fx::ZERO, fx::ZERO } == origin);
  }

  TEST_CASE("vec2: addition and subtraction") {
    const vec2 a = whole(3, -4);
    const vec2 b = whole(-7, 2);
    const vec2 zero{};

    CHECK(a + zero == a);
    CHECK(a - zero == a);
    CHECK(a - a == zero);
    CHECK(a + b == b + a);
    CHECK(a + (-a) == zero);
    CHECK(-(-a) == a);
    CHECK(a - b == a + (-b));

    // associativity, which holds here because nothing overflows
    const vec2 c = whole(11, 13);
    CHECK((a + b) + c == a + (b + c));

    // compound forms must agree with the binary ones or the two spellings
    // drift, the same failure the fx suite guards against
    vec2 acc = a;
    acc += b;
    CHECK(acc == a + b);
    acc -= b;
    CHECK(acc == a);
  }

  TEST_CASE("vec2: scaling by an fx") {
    const vec2 v = whole(3, -4);

    CHECK(v * fx::ONE == v);
    CHECK(v * fx::ZERO == vec2{});
    CHECK(v * fx::from_int(2) == v + v);
    CHECK(v * fx::from_int(-1) == -v);
    CHECK(v / fx::ONE == v);
    CHECK(v / fx::from_int(2) == v * fx::from_ratio(1, 2));

    // scaling is componentwise, so it distributes over addition
    const vec2 w = whole(11, 5);
    const fx   k = fx::from_int(3);
    CHECK((v + w) * k == v * k + w * k);
  }

  TEST_CASE("vec2: dot is symmetric and agrees with length_sq") {
    const vec2 a = whole(3, 4);
    const vec2 b = whole(-2, 7);

    CHECK(dot(a, b) == dot(b, a));
    CHECK(dot(a, vec2{}) == fx64::ZERO);

    // dot(v, v) IS length_sq(v). They disagreed before dot returned fx64:
    // at 200 units dot gave -25536 where length_sq gave 40000.
    CHECK(dot(a, a) == length_sq(a));
    const vec2 far = whole(200, 0);
    CHECK(dot(far, far) == length_sq(far));
    CHECK(length_sq(far) == fx64::from_int(40000));

    // axis vectors select a component, and perpendicular vectors give zero
    const vec2 ex = whole(1, 0);
    const vec2 ey = whole(0, 1);
    CHECK(dot(a, ex) == fx64::widen(a.x));
    CHECK(dot(a, ey) == fx64::widen(a.y));
    CHECK(dot(ex, ey) == fx64::ZERO);

    // bilinear in the second argument
    const vec2 c = whole(5, -1);
    CHECK(dot(a, b + c) == dot(a, b) + dot(a, c));
    CHECK(dot(a, b * fx::from_int(3)) == dot(a, b) * fx::from_int(3));
  }

  TEST_CASE("vec2: cross is antisymmetric") {
    const vec2 a  = whole(3, 4);
    const vec2 b  = whole(-2, 7);
    const vec2 ex = whole(1, 0);
    const vec2 ey = whole(0, 1);

    CHECK(cross(a, b) == -cross(b, a));
    CHECK(cross(a, a) == fx64::ZERO);
    CHECK(cross(a, vec2{}) == fx64::ZERO);

    // the unit square has area 1, with the sign giving orientation
    CHECK(cross(ex, ey) == fx64::ONE);
    CHECK(cross(ey, ex) == -fx64::ONE);

    // parallel vectors span no area
    CHECK(cross(a, a * fx::from_int(3)) == fx64::ZERO);

    // sign says which side of the line through the origin along a point lies
    CHECK(sign(cross(ex, whole(1, 1))) == 1);
    CHECK(sign(cross(ex, whole(1, -1))) == -1);
    CHECK(sign(cross(ex, whole(5, 0))) == 0);
  }

  TEST_CASE("vec2: dot and cross against an exact reference") {
    // The reference is computed from the raws in i128 and shares no code with
    // vec2.hpp. mul_wide is exact, so the answers must match bit for bit.
    tally d{};
    tally c{};
    for (i32 ax = -2000; ax <= 2000; ax += 131) {
      for (i32 ay = -2000; ay <= 2000; ay += 137) {
        for (i32 bx = -2000; bx <= 2000; bx += 149) {
          for (i32 by = -2000; by <= 2000; by += 151) {
            const vec2 p = whole(ax, ay);
            const vec2 q = whole(bx, by);
            d.probe(ref_dot(p, q) == i128{ dot(p, q).raw });
            c.probe(ref_cross(p, q) == i128{ cross(p, q).raw });
          }
        }
      }
    }
    CHECK(d.checked == 677970);  // 31 * 30 * 27 * 27
    CHECK(d.failures == 0);
    CHECK(c.checked == 677970);
    CHECK(c.failures == 0);
  }

  TEST_CASE("vec2: distance_sq is the squared distance") {
    // The regression, by value. The old form computed a.a - b.b, which gave 8
    // for a distance of 2, went negative when b was the further point, and was
    // not symmetric.
    const vec2 a = whole(3, 0);
    const vec2 b = whole(1, 0);
    CHECK(distance_sq(a, b) == fx64::from_int(4));
    CHECK(distance_sq(b, a) == fx64::from_int(4));
    CHECK(distance_sq(a, b).raw > 0);

    const vec2 p = whole(3, 4);
    CHECK(distance_sq(p, vec2{}) == fx64::from_int(25));
    CHECK(distance_sq(p, p) == fx64::ZERO);

    // it is length_sq of the difference, by definition and by construction
    const vec2 q = whole(-7, 11);
    CHECK(distance_sq(p, q) == length_sq(p - q));
  }

  TEST_CASE("vec2: distance_sq against an exact reference") {
    tally t{};
    tally sym{};
    tally nonneg{};
    for (i32 ax = -2000; ax <= 2000; ax += 149) {
      for (i32 ay = -2000; ay <= 2000; ay += 151) {
        for (i32 bx = -2000; bx <= 2000; bx += 157) {
          for (i32 by = -2000; by <= 2000; by += 163) {
            const vec2 p = whole(ax, ay);
            const vec2 q = whole(bx, by);
            const fx64 d = distance_sq(p, q);
            t.probe(ref_distance_sq(p, q) == i128{ d.raw });
            sym.probe(d == distance_sq(q, p));
            nonneg.probe(d.raw >= 0);
          }
        }
      }
    }
    CHECK(t.checked == 473850);  // 27 * 27 * 26 * 25
    CHECK(t.failures == 0);
    CHECK(sym.failures == 0);
    CHECK(nonneg.failures == 0);
  }

  TEST_CASE("vec2: length is exact on whole answers") {
    CHECK(length(whole(3, 4)) == fx::from_int(5));
    CHECK(length(whole(-3, 4)) == fx::from_int(5));
    CHECK(length(whole(3, -4)) == fx::from_int(5));
    CHECK(length(whole(5, 12)) == fx::from_int(13));
    CHECK(length(whole(8, 15)) == fx::from_int(17));
    CHECK(length(vec2{}) == fx::ZERO);

    // along an axis the length is the coordinate, with no rounding at all
    for (i32 k = 0; k <= 1000; ++k) {
      CHECK(length(whole(k, 0)) == fx::from_int(k));
      CHECK(length(whole(0, -k)) == fx::from_int(k));
    }
  }

  TEST_CASE("vec2: length is the floor of the true length") {
    // Same postcondition as isqrt restated in fx terms: the result squared may
    // not exceed the squared length, and one ULP more must exceed it.
    // mul_wide is what keeps this exact.
    tally low{};
    tally high{};
    for (i32 x = -400; x <= 400; x += 7) {
      for (i32 y = -400; y <= 400; y += 11) {
        const vec2 v   = whole(x, y);
        const fx   len = length(v);
        const fx   up  = fx::from_raw(len.raw + 1);
        low.probe(mul_wide(len, len).raw <= length_sq(v).raw);
        high.probe(mul_wide(up, up).raw > length_sq(v).raw);
      }
    }
    CHECK(low.checked == 8395);  // 115 * 73
    CHECK(low.failures == 0);
    CHECK(high.checked == 8395);
    CHECK(high.failures == 0);
  }

  TEST_CASE("vec2: length never exceeds i32 inside its documented range") {
    // The narrowing regression. isqrt returns u32 and length_sq reaches 2^63,
    // so the root reaches about 3.03e9; length((30000 30000)) came back as raw
    // -1514510296 before the assert existed. Above fx::MAX it now aborts in
    // debug rather than wrapping, so what is testable here is the boundary
    // that still fits.
    CHECK(length(whole(32767, 0)).raw == 32767 * 65536);
    CHECK(length(whole(32767, 0)).raw > 0);
    CHECK(length(whole(23000, 23000)).raw > 0);
    CHECK(length(whole(23000, 23000)).raw < I32_MAX);

    // and it is non-negative and non-decreasing along a ray
    i32 prev = 0;
    for (i32 k = 0; k <= 20000; k += 37) {
      const i32 r = length(whole(k, k)).raw;
      CHECK(r >= prev);
      prev = r;
    }
  }

  TEST_CASE("vec2: length agrees with sqrt of the squared length") {
    // Two routes to the same number: isqrt straight off the Q32.32 raw, and
    // sqrt(fx) after narrowing. They agree wherever the narrowing is lossless,
    // which is the whole point of length_sq being fx64.
    tally t{};
    for (i32 k = 0; k <= 181; ++k) {
      const vec2 v = whole(k, 0);
      t.probe(length(v) == sqrt(narrow(length_sq(v))));
    }
    CHECK(t.checked == 182);
    CHECK(t.failures == 0);
  }

  TEST_CASE("vec2: normalise") {
    // exact on the axes
    CHECK(normalise(whole(5, 0)) == whole(1, 0));
    CHECK(normalise(whole(0, -9)) == whole(0, -1));

    // and close to unit length elsewhere. length floors, so the result can be
    // a shade long; one ULP of slack on a Q16.16 length is the tolerance.
    for (i32 k = 1; k <= 200; ++k) {
      const vec2 n = normalise(whole(3 * k, 4 * k));
      const i32  l = length(n).raw;
      CHECK(l >= fx::ONE.raw - 4);
      CHECK(l <= fx::ONE.raw + 4);
    }
  }

  TEST_CASE("vec2: constexpr") {
    // the whole surface is usable at compile time, so a constant vector costs
    // nothing at runtime
    constexpr vec2 a{ fx::from_int(3), fx::from_int(4) };
    constexpr vec2 b{ fx::from_int(1), fx::from_int(2) };

    static_assert((a + b).x.raw == fx::from_int(4).raw);
    static_assert((a - b).y.raw == fx::from_int(2).raw);
    static_assert((-a).x.raw == fx::from_int(-3).raw);
    static_assert(dot(a, b).raw == fx64::from_int(11).raw);
    static_assert(cross(a, b).raw == fx64::from_int(2).raw);
    static_assert(length_sq(a).raw == fx64::from_int(25).raw);
    static_assert(length(a).raw == fx::from_int(5).raw);
    static_assert(distance_sq(a, b).raw == fx64::from_int(8).raw);
    static_assert(a == a);
    CHECK(true);
  }

  TEST_CASE("vec2: byte representation") {
    // vec2 goes into the rollback snapshot and the desync fingerprint, both of
    // which read it as a plain block of bytes. Two vec2 that compare equal
    // must therefore be equal byte for byte, which rules out padding.
    static_assert(sizeof(vec2) == 8);
    static_assert(alignof(vec2) == 4);
    static_assert(std::is_trivially_copyable_v<vec2>);
    static_assert(std::is_standard_layout_v<vec2>);
    static_assert(std::has_unique_object_representations_v<vec2>);
    static_assert(!std::is_aggregate_v<vec2>);  // the explicit constructor
    static_assert(sizeof(vec2) == 2 * sizeof(fx));
    CHECK(sizeof(vec2) == 8);
  }

  TEST_CASE("vec2: scaling commutes") {
    // `2_fx * v` and `v * 2_fx` must be the same value, not merely the same
    // idea. The commuted form delegates for exactly this reason.
    u32 rng = 0x2468ACE0;
    u64 checked = 0;
    u64 failures = 0;
    for (i32 i = 0; i < 20000; ++i) {
      rng          = rng * 1664525u + 1013904223u;
      const fx   s = fx::from_raw(static_cast<i32>(rng >> 12));
      rng          = rng * 1664525u + 1013904223u;
      const i32  a = static_cast<i32>(rng >> 14) - (1 << 17);
      rng          = rng * 1664525u + 1013904223u;
      const i32  b = static_cast<i32>(rng >> 14) - (1 << 17);
      const vec2 v{ fx::from_raw(a), fx::from_raw(b) };
      ++checked;
      if (!(s * v == v * s)) ++failures;
    }
    CHECK(checked == 20000);
    CHECK(failures == 0);
  }

  TEST_CASE("vec2: perp is the anticlockwise quarter turn") {
    // The DIRECTION is fixed by an identity rather than recorded by an
    // example. `cross(a, b)` is positive when b lies anticlockwise of a, and
    // perp(v) is a quarter turn of the same length, so the cross product is
    // exactly length_sq(v) -- positive for every non-zero v. The clockwise
    // version would give -length_sq(v), and this case is what would fail.
    u32 rng      = 0x13579BDF;
    u64 checked  = 0;
    u64 failures = 0;
    for (i32 i = 0; i < 20000; ++i) {
      rng           = rng * 1664525u + 1013904223u;
      const i32 x   = static_cast<i32>(rng >> 10) - (1 << 21);
      rng           = rng * 1664525u + 1013904223u;
      const i32 y   = static_cast<i32>(rng >> 10) - (1 << 21);
      const vec2 v{ fx::from_raw(x), fx::from_raw(y) };
      const vec2 p = perp(v);
      ++checked;
      const bool ok = cross(v, p) == length_sq(v)      // anticlockwise
                      && dot(v, p) == fx64::ZERO       // a right angle
                      && length_sq(p) == length_sq(v)  // same length
                      && perp(p) == -v;                // twice is a half turn
      if (!ok) ++failures;
    }
    CHECK(checked == 20000);
    CHECK(failures == 0);

    // The worked example, so a failure above is readable.
    const vec2 east{ fx::from_int(1), fx::from_int(0) };
    CHECK(perp(east) == vec2{ fx::from_int(0), fx::from_int(1) });
  }

  TEST_CASE("vec2: golden hash sequence") {
    // Same construction as the fx, fx64 and angle hashes: ~50k mixed
    // operations folded into one committed constant that every CI row checks.
    // Covers dot, cross, length_sq, distance_sq, length, normalise, perp, the
    // commuted scale and the arithmetic operators -- so a change to any of
    // them moves it.
    u32  hash = 0x811c9dc5;
    auto fnv  = [&hash](i32 raw) {
      hash ^= static_cast<u32>(raw);
      hash *= 0x01000193;
    };
    auto fnv64 = [&fnv](i64 raw) {
      fnv(static_cast<i32>(static_cast<u64>(raw) & 0xffffffffu));
      fnv(static_cast<i32>(static_cast<u64>(raw) >> 32));
    };
    u32  rng  = 0x12345678;
    auto nraw = [&rng]() -> i32 {
      rng = rng * 1664525u + 1013904223u;
      return static_cast<i32>(rng >> 9) - (1 << 22);  // +/- 64 in fx value
    };
    auto nv = [&nraw]() { return vec2{ fx::from_raw(nraw()), fx::from_raw(nraw()) }; };

    for (i32 i = 0; i < 50000; ++i) {
      const vec2 a = nv(), b = nv();
      fnv64(dot(a, b).raw);
      fnv64(cross(a, b).raw);
      fnv64(length_sq(a).raw);
      fnv64(distance_sq(a, b).raw);
      fnv((a + b).x.raw);
      fnv((a - b).y.raw);
      const vec2 p = perp(a);
      fnv(p.x.raw);
      fnv(p.y.raw);
      fnv(length(a).raw);
      const vec2 s = fx::from_ratio(1, 3) * a;
      fnv(s.x.raw);
      fnv(s.y.raw);
      if (i % 7 == 0 && a != vec2::ZERO) {
        const vec2 n = normalise(a);
        fnv(n.x.raw);
        fnv(n.y.raw);
      }
    }

    CHECK(hash == 0x32AE149C);
  }
}
