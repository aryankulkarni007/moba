#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/isqrt.hpp>

// main() comes from tests/doctest_main.cpp.

// covers <moba/fx/isqrt.hpp>.

// The first case is the whole specification; everything after it covers values
// a sweep might miss, or the fixed-point wrapping rather than the root itself.

namespace {

using namespace moba;

/// r == floor(sqrt(n)), stated as the interval it must land in.
///
/// u128 is not optional here: r reaches 2^32-1, so (r+1)^2 reaches 2^64 and
/// overflows the same u64 that n arrived in. Computing this in u64 would wrap
/// to 0 and the check would pass on everything.
[[nodiscard]] constexpr bool is_floor_sqrt(u64 n, u64 r) noexcept {
  const u128 lo = u128{ r } * r;
  const u128 hi = (u128{ r } + 1) * (u128{ r } + 1);
  return lo <= n && hi > n;
}

/// Accumulates over a sweep instead of a CHECK per value: a CHECK per input
/// across ~500k probes is slow and, on a real regression, unreadable.
struct sweep {
  u64 checked   = 0;
  u64 failures  = 0;
  u64 first_bad = 0;

  constexpr void probe(u64 n) noexcept {
    ++checked;
    if (!is_floor_sqrt(n, isqrt(n))) {
      if (failures == 0) first_bad = n;
      ++failures;
    }
  }
};
}  // namespace

TEST_SUITE("fx/isqrt") {
  TEST_CASE("isqrt: postcondition") {
    // r*r <= n < (r+1)*(r+1) is the definition of the integer square root
    // The intervals [k^2, (k+1)^2) tile the non-negative integers with no gap
    // and no overlap, so every n falls in exactly one and its k is
    // floor(sqrt(n)). An implementation that satisfies this on
    // every input is correct on every input - which is why this case is the
    // spine of the file and the rest is coverage of values a sweep can miss.
    sweep s{};

    SUBCASE("dense low range") {
      for (u64 n = 0; n < 200000; ++n) s.probe(n);
      CHECK(s.checked == 200000);
    }

    SUBCASE("every k^2-1, k^2, k^2+1") {
      // The boundaries are the only places the answer changes, so this is
      // where an off-by-one lives. Nowhere else can one.
      for (u64 k = 0; k < 150000; ++k) {
        const u64 sq = k * k;
        if (sq > 0) s.probe(sq - 1);
        s.probe(sq);
        s.probe(sq + 1);
      }
      CHECK(s.checked > 400000);
    }

    SUBCASE("large k^2-1, k^2, k^2+1 - past 2^53") {
      // Above 2^53 a double cannot represent these integers, so this range is
      // exactly where a floating-point implementation stops being exact.
      for (u64 k = 0; k < 100000; ++k) {
        const u64 r  = 4000000000ULL + k;
        const u64 sq = r * r;
        s.probe(sq - 1);
        s.probe(sq);
        s.probe(sq + 1);
      }
      CHECK(s.checked == 300000);
    }

    SUBCASE("every power of two, and its neighbours") {
      for (int i = 0; i < 64; ++i) {
        const u64 p = u64{ 1 } << i;
        if (p > 0) s.probe(p - 1);
        s.probe(p);
        s.probe(p + 1);
      }
      CHECK(s.checked == 192);
    }

    SUBCASE("strided across the whole u64 range") {
      // The dense sweeps above never leave the bottom of the domain. This one
      // reaches the top, where bit starts at 4^31 and never idles.
      const u64 stride = 0xFFFFFFFFFFFFFFFFULL / 60000;
      for (u64 i = 0; i < 60000; ++i) s.probe(i * stride);
      CHECK(s.checked == 60000);
    }

    SUBCASE("the extremes") {
      s.probe(0);
      s.probe(1);
      s.probe(u64{ 1 } << 62);
      s.probe(u64{ 1 } << 63);
      s.probe(0xFFFFFFFFFFFFFFFEULL);
      s.probe(0xFFFFFFFFFFFFFFFFULL);
      CHECK(s.checked == 6);
    }

    CHECK(s.failures == 0);
    if (s.failures != 0) {
      MESSAGE(
          "first failing n = " << s.first_bad << ", isqrt returned "
                               << isqrt(s.first_bad)
      );
    }
  }

  TEST_CASE("isqrt: perfect squares are exact") {
    // The case a float implementation gets wrong, and the one that makes
    // length() of an axis-aligned vector come out clean rather than one ULP
    // short. Swept up to the largest k whose square still fits u64.
    for (u64 k = 0; k < 100000; ++k) CHECK(isqrt(k * k) == k);

    // the top of the domain: (2^32-1)^2 is the largest perfect square in u64
    constexpr u64 top = 4294967295ULL;
    CHECK(isqrt(top * top) == top);
    CHECK(isqrt((top * top) - 1) == top - 1);

    for (int i = 0; i < 32; ++i) {
      const u64 k = u64{ 1 } << i;
      CHECK(isqrt(k * k) == k);
    }
  }

  TEST_CASE("isqrt: named boundaries") {
    // constexpr, so these hold at compile time or the file does not build.
    static_assert(isqrt(0) == 0);
    static_assert(isqrt(1) == 1);
    static_assert(isqrt(2) == 1);
    static_assert(isqrt(3) == 1);
    static_assert(isqrt(4) == 2);
    static_assert(isqrt(8) == 2);
    static_assert(isqrt(9) == 3);
    static_assert(isqrt(15) == 3);
    static_assert(isqrt(16) == 4);
    static_assert(isqrt(168) == 12);
    static_assert(isqrt(169) == 13);
    static_assert(isqrt(170) == 13);

    // the result is u32 because the domain demands it: the largest answer is
    // exactly 2^32-1, which does NOT fit i32. Only sqrt(fx) may narrow.
    static_assert(isqrt(0xFFFFFFFFFFFFFFFFULL) == 4294967295U);
    CHECK(isqrt(0xFFFFFFFFFFFFFFFFULL) > static_cast<u32>(I32_MAX));
  }

  TEST_CASE("isqrt: is monotonic non-decreasing") {
    // Cheap, and it catches a whole class of bugs the postcondition sweep
    // could only catch by landing on the exact input: a start value that is
    // occasionally wrong shows up as a dip.
    u32 prev = 0;
    for (u64 n = 0; n < 300000; ++n) {
      const u32 r = isqrt(n);
      CHECK(r >= prev);
      prev = r;
    }
  }

  TEST_CASE("isqrt: a double-based implementation is wrong here") {
    // not a comparison against a reference - a demonstration of why there is
    // no floating-point reference to compare against.
    // This documents the trap
    // in the place someone would come to "simplify" the loop away.
    //
    // n = 2^54 - 1. A double carries a 53-bit mantissa, so converting n rounds
    // it UP to exactly 2^54, whose square root is exactly 2^27. The naive
    // implementation therefore returns a value whose square EXCEEDS n.
    constexpr u64 n = (u64{ 1 } << 54) - 1;

    static_assert(isqrt(n) == 134217727U);      // 2^27 - 1, correct
    static_assert(is_floor_sqrt(n, isqrt(n)));  // and it satisfies the spec

    // what the double route produces, spelled out rather than computed, so
    // this case cannot itself drift with the libm:
    constexpr u64 via_double = 134217728U;  // 2^27
    static_assert(via_double * via_double > n);
    static_assert(!is_floor_sqrt(n, via_double));
    CHECK(isqrt(n) != via_double);
  }
}

TEST_SUITE("fx/sqrt") {
  TEST_CASE("isqrt: known values") {
    CHECK(sqrt(fx::from_int(4)) == fx::from_int(2));
    CHECK(sqrt(fx::from_int(169)) == fx::from_int(13));
    CHECK(sqrt(fx::from_int(100)) == fx::from_int(10));
    CHECK(sqrt(fx::from_int(1)) == fx::ONE);
    CHECK(sqrt(fx::from_ratio(1, 4)) == fx::from_ratio(1, 2));
    CHECK(sqrt(fx::from_ratio(1, 16)) == fx::from_ratio(1, 4));

    // exact whole roots stay exact - no off-by-one ULP, which is the failure
    // a float implementation produces and the reason for integer arithmetic.
    for (i32 k = 0; k <= 181; ++k) {
      CHECK(sqrt(fx::from_int(k * k)) == fx::from_int(k));
    }

    // and an irrational, floored like everything else in the library
    CHECK(sqrt(fx::from_int(2)).raw == 92681);  // 1.41420... , floor of 1.41421
  }

  TEST_CASE("isqrt: the fixed-point postcondition") {
    // The same interval property as isqrt, restated in fx terms: the result
    // squared must not exceed the input, and one ULP more must exceed it.
    //
    // mul_wide is what makes this exact - r*r as an fx would overflow above
    // r ~ 181, which is the entire range of interest here.
    i64 failures = 0;
    i64 checked  = 0;
    for (i64 r = 0; r <= I32_MAX; r += 65413) {
      const fx v  = fx::from_raw(static_cast<i32>(r));
      const fx s  = sqrt(v);
      const fx up = fx::from_raw(s.raw + 1);

      const i64 widened = fx64::widen(v).raw;
      ++checked;
      if (mul_wide(s, s).raw > widened) ++failures;
      if (mul_wide(up, up).raw <= widened) ++failures;
    }
    CHECK(checked > 32000);
    CHECK(failures == 0);
  }

  TEST_CASE("isqrt: Q32.32 in and Q16.16 out") {
    // The scaling identity the whole header is built on, checked in both
    // spellings. A square root halves the scale factor, so a Q32.32 raw is
    // exactly what produces a Q16.16 raw - which is why vec2::length will
    // need no shift at all and sqrt(fx) needs one.
    for (i32 k = 1; k <= 181; ++k) {
      const fx   root = fx::from_int(k);
      const fx64 sq   = mul_wide(root, root);  // k^2 as Q32.32

      // the vec2::length path: isqrt straight off the fx64 raw, no shift
      CHECK(
          fx::from_raw(static_cast<i32>(isqrt(static_cast<u64>(sq.raw))))
          == root
      );

      // the sqrt(fx) path: same answer via the narrow type, which has to
      // widen first
      CHECK(sqrt(narrow(sq)) == root);
    }

    // widen() IS the shift, spelled two ways
    const fx v = fx::from_int(2);
    CHECK(fx64::widen(v).raw == static_cast<i64>(v.raw) << 16);
  }

  TEST_CASE("isqrt: zero and negative") {
    CHECK(sqrt(fx::ZERO) == fx::ZERO);
    CHECK(sqrt(fx::EPSILON).raw == 256);  // sqrt(2^-16) is 2^-8, raw 2^8

#if defined(NDEBUG)
    // negative input has no answer, so it gets a DEFINED one rather than
    // whatever the arithmetic happens to do. Debug asserts instead; this is
    // the release arm, and both machines must agree on it.
    CHECK(sqrt(fx::from_int(-1)) == fx::ZERO);
    CHECK(sqrt(fx::from_raw(-1)) == fx::ZERO);
    CHECK(sqrt(fx::MIN) == fx::ZERO);
#endif
  }

  TEST_CASE("isqrt: is monotonic and never exceeds i32") {
    // sqrt(fx) is total on non-negative input: the worst case is fx::MAX
    // widened, whose root is 11863283 - 181x inside i32. That is why the
    // narrowing cast in sqrt() needs no range check
    fx prev = fx::ZERO;
    for (i64 r = 0; r <= I32_MAX; r += 65413) {
      const fx s = sqrt(fx::from_raw(static_cast<i32>(r)));
      CHECK(s.raw >= prev.raw);
      prev = s;
    }
    CHECK(sqrt(fx::MAX).raw == 11863283);
    CHECK(sqrt(fx::MAX).raw < I32_MAX);
  }

  TEST_CASE("isqrt: constexpr") {
    // proves constexpr is not just a label
    // named constants should cost nothing at runtime
    static_assert(sqrt(fx::from_int(4)).raw == fx::from_int(2).raw);
    static_assert(sqrt(fx::from_int(169)).raw == fx::from_int(13).raw);
    static_assert(sqrt(fx::ZERO).raw == 0);
    static_assert(sqrt(fx::ONE).raw == fx::ONE.raw);
    CHECK(true);
  }
}
