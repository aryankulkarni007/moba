#include <cmath>
#include <format>
#include <doctest/doctest.h>
#include <moba/fx/angle.hpp>
#include <moba/fx/format.hpp>
#include <numbers>

// main() comes from tests/doctest_main.cpp.

// covers <moba/fx/angle.hpp>.

// two independent references run here and they catch different failures.
//
//   std::sin / std::cos / std::atan2 in double answer "is the value RIGHT",
//   and would catch a plausible-but-wrong CORDIC.
//
//   the algebraic identities -- antisymmetry, sin^2+cos^2, the atan2 round
//   trip -- answer "is it SELF-consistent", and would catch an error the
//   double reference shares, or one that only shows at a fold boundary.
//
// double is banned in the library and fine here: nothing in this file feeds a
// golden hash, and every comparison against it is a stated tolerance in LSBs
// rather than an equality.
//
// The sweeps over `sin` and `cos` visit all 65536 representable angles, which
// is the ENTIRE input domain of those functions. Those cases are proofs, not
// samples, which is why they assert the exact visit count -- a loop that stops
// early must fail rather than report a clean run over fewer inputs.
//
// Every case name here is prefixed `angle: `, as test_fx64.cpp prefixes its
// own. ctest registers one entry per TEST_CASE under the case name ALONE, so
// two suites in the same binary sharing a name collapse into one ambiguous
// entry that runs both -- which is how "golden hash sequence" first landed.
//
// TODO: [missing] atan2(0, 0) asserts, and that arm is untested. It needs the
//       death-test harness (see cmake/MobaDeathTest.cmake and
//       engine/core/tests/death_assert.cpp), not a CHECK.

namespace {

using namespace moba;

constexpr double PI   = std::numbers::pi;
constexpr double TURN = 65536.0;  // raw units in a full turn, both BAM and fx

[[nodiscard]] double as_double(fx v) noexcept {
  return static_cast<double>(v.raw) / TURN;
}
[[nodiscard]] double as_radians(angle a) noexcept {
  return static_cast<double>(a.raw) * (2.0 * PI / TURN);
}

/// fold a radian difference into [-pi, pi]. comparing angles by subtraction
/// without this reports a full turn of error for two angles either side of
/// zero, which is no error at all.
[[nodiscard]] double wrap_pi(double r) noexcept {
  while (r > PI) r -= 2.0 * PI;
  while (r < -PI) r += 2.0 * PI;
  return r;
}

/// accumulates the worst deviation over a sweep instead of issuing a check per
/// value: 65536 CHECKs is slow and, on a real regression, unreadable. Keeps
/// the input that produced the worst case so a failure names a reproducer.
struct deviation {
  u64    checked  = 0;
  double worst    = 0.0;
  i64    worst_at = 0;

  void probe(i64 at, double got, double want) noexcept {
    ++checked;
    const double e = got > want ? got - want : want - got;
    if (e > worst) {
      worst    = e;
      worst_at = at;
    }
  }

  /// worst deviation expressed in fx LSBs, which is the unit the tolerances
  /// in this file are stated in.
  [[nodiscard]] double worst_lsb() const noexcept { return worst * TURN; }
};

/// counts exact-equality failures over a sweep, same reasoning as `deviation`.
struct mismatch {
  u64 checked   = 0;
  u64 failures  = 0;
  i64 first_bad = 0;

  void probe(i64 at, bool ok) noexcept {
    ++checked;
    if (!ok) {
      if (failures == 0) first_bad = at;
      ++failures;
    }
  }
};

}  // namespace

TEST_SUITE("fx/angle") {
  TEST_CASE("angle: representation") {
    // the whole point of binary angle measure: the useful angles are exact
    // integers, not approximations of pi.
    CHECK(angle::SCALE == 65536);
    CHECK(angle::ZERO.raw == 0);
    CHECK(angle::QUARTER.raw == 16384);
    CHECK(angle::HALF.raw == 32768);
    CHECK(angle::THREE_QUARTER.raw == 49152);

    // a full turn is zero. this is why scale cannot be an angle constant, and
    // it is the property that makes wraparound free.
    CHECK(angle::QUARTER + angle::THREE_QUARTER == angle::ZERO);
    CHECK(angle::HALF + angle::HALF == angle::ZERO);
  }

  TEST_CASE("angle: conversion") {
    SUBCASE("turns are exact") {
      static_assert(angle::from_turns(0, 1) == angle::ZERO);
      static_assert(angle::from_turns(1, 4) == angle::QUARTER);
      static_assert(angle::from_turns(1, 2) == angle::HALF);
      static_assert(angle::from_turns(3, 4) == angle::THREE_QUARTER);
      static_assert(angle::from_turns(1, 1) == angle::ZERO);
      static_assert(angle::from_turns(-1, 4) == angle::THREE_QUARTER);
      CHECK(angle::from_turns(1, 8).raw == 8192);
    }

    SUBCASE("degrees are exact only on multiples of 45") {
      // 65536/360 is not an integer, so only the eighths land exactly.
      CHECK(angle::from_degrees(0) == angle::ZERO);
      CHECK(angle::from_degrees(45).raw == 8192);
      CHECK(angle::from_degrees(90) == angle::QUARTER);
      CHECK(angle::from_degrees(180) == angle::HALF);
      CHECK(angle::from_degrees(270) == angle::THREE_QUARTER);
      CHECK(angle::from_degrees(360) == angle::ZERO);
      CHECK(angle::from_degrees(-90) == angle::THREE_QUARTER);

      // 1 degree is 182.04 raw units; the policy is to floor, so 182.
      CHECK(angle::from_degrees(1).raw == 182);
    }
  }

  TEST_CASE("angle: arithmetic wraps and never clamps") {
    const angle a = angle::from_degrees(350);
    const angle b = angle::from_degrees(20);

    // Past a full turn, round the circle rather than saturating at the top.
    CHECK((a + b).raw < a.raw);
    CHECK(a - a == angle::ZERO);
    CHECK(-angle::ZERO == angle::ZERO);
    CHECK(-angle::QUARTER == angle::THREE_QUARTER);

    angle c = a;
    c += b;
    CHECK(c == a + b);
    c -= b;
    CHECK(c == a);

    // exhaustive: for every representable angle, negation and subtraction
    // agree, and adding a full turn's worth of steps returns to the start.
    mismatch m{};
    for (i32 r = 0; r < 65536; ++r) {
      const angle x = angle::from_raw(static_cast<u16>(r));
      m.probe(r, (x + -x) == angle::ZERO && (angle::ZERO - x) == -x);
    }
    CHECK(m.checked == 65536);
    CHECK(m.failures == 0);
  }

  TEST_CASE("angle: shortest delta") {
    // The reason this function exists: raw subtraction says 350 -> 10 is a
    // journey of 340 degrees. It is 20.
    CHECK(
        shortest_delta(angle::from_degrees(350), angle::from_degrees(10)) > 0
    );
    CHECK(
        shortest_delta(angle::from_degrees(10), angle::from_degrees(350)) < 0
    );
    CHECK(shortest_delta(angle::ZERO, angle::ZERO) == 0);
    CHECK(shortest_delta(angle::ZERO, angle::QUARTER) == 16384);
    CHECK(shortest_delta(angle::QUARTER, angle::ZERO) == -16384);

    // EXACTLY OPPOSITE ANGLES. A half turn is 32768, which does not fit i16,
    // so it lands on -32768: opposite angles always turn the same way. Pinned
    // by value because two call sites disagreeing about it would be a desync.
    CHECK(shortest_delta(angle::ZERO, angle::HALF) == -32768);
    CHECK(shortest_delta(angle::HALF, angle::ZERO) == -32768);

    // Antisymmetric everywhere except that one ambiguous point, and never
    // longer than half a turn.
    mismatch anti{};
    mismatch bound{};
    for (i32 r = 0; r < 65536; ++r) {
      const angle x = angle::from_raw(static_cast<u16>(r));
      const int   d = shortest_delta(angle::ZERO, x);
      bound.probe(r, d >= -32768 && d <= 32767);
      if (x != angle::HALF) {
        anti.probe(r, d == -shortest_delta(x, angle::ZERO));
      }
    }
    CHECK(bound.checked == 65536);
    CHECK(bound.failures == 0);
    CHECK(anti.checked == 65535);  // every angle but the half turn
    CHECK(anti.failures == 0);
  }

  TEST_CASE("angle: cardinals are exact") {
    // Not a tolerance. Gameplay leans on a character facing east having
    // velocity exactly (speed, 0), and one LSB of error there compounds every
    // tick, so these four are pinned in the table rather than computed.
    CHECK(sin(angle::ZERO).raw == 0);
    CHECK(sin(angle::QUARTER).raw == 65536);
    CHECK(sin(angle::HALF).raw == 0);
    CHECK(sin(angle::THREE_QUARTER).raw == -65536);

    CHECK(cos(angle::ZERO).raw == 65536);
    CHECK(cos(angle::QUARTER).raw == 0);
    CHECK(cos(angle::HALF).raw == -65536);
    CHECK(cos(angle::THREE_QUARTER).raw == 0);
  }

  TEST_CASE("angle: sin and cos against double") {
    deviation s{};
    deviation c{};
    for (i32 r = 0; r < 65536; ++r) {
      const angle  a = angle::from_raw(static_cast<u16>(r));
      const double t = as_radians(a);
      s.probe(r, as_double(sin(a)), std::sin(t));
      c.probe(r, as_double(cos(a)), std::cos(t));
    }

    // The entire input domain, so this is a proof rather than a sample.
    CHECK(s.checked == 65536);
    CHECK(c.checked == 65536);

    INFO("worst sin ", s.worst_lsb(), " LSB at raw ", s.worst_at);
    INFO("worst cos ", c.worst_lsb(), " LSB at raw ", c.worst_at);

    // Measured 0.81 LSB. The budget is interpolation error (about a third of
    // an LSB) plus one round-to-nearest narrowing (half an LSB). Anything
    // approaching 2 LSB means a second rounding has crept back in.
    CHECK(s.worst_lsb() < 1.0);
    CHECK(c.worst_lsb() < 1.0);
  }

  TEST_CASE("angle: sin never leaves the unit interval") {
    // A linear interpolation between table entries can only undershoot a
    // concave curve, but the round-to-nearest narrowing could in principle
    // push a value past 1.0 -- and a sine above 1 makes a unit vector longer
    // than one unit, which breaks every normalisation downstream.
    mismatch m{};
    for (i32 r = 0; r < 65536; ++r) {
      const angle a = angle::from_raw(static_cast<u16>(r));
      m.probe(
          r,
          sin(a).raw <= 65536 && sin(a).raw >= -65536 && cos(a).raw <= 65536
              && cos(a).raw >= -65536
      );
    }
    CHECK(m.checked == 65536);
    CHECK(m.failures == 0);
  }

  TEST_CASE("angle: trig identities") {
    SUBCASE("pythagorean") {
      deviation d{};
      for (i32 r = 0; r < 65536; ++r) {
        const angle  a = angle::from_raw(static_cast<u16>(r));
        const double s = as_double(sin(a));
        const double c = as_double(cos(a));
        d.probe(r, (s * s) + (c * c), 1.0);
      }
      CHECK(d.checked == 65536);
      INFO("worst sin^2+cos^2-1 ", d.worst, " at raw ", d.worst_at);
      // Two values each carrying up to 0.81 LSB, squared and summed.
      CHECK(d.worst_lsb() < 2.5);
    }

    SUBCASE("sin is exactly antisymmetric") {
      // EXACT, not within a tolerance. It holds because there is exactly one
      // rounding left in the whole path -- an earlier version with two
      // roundings failed this on 60928 of the 65536 angles, which is what
      // caught the redundant narrowing.
      mismatch m{};
      for (i32 r = 0; r < 65536; ++r) {
        const angle a = angle::from_raw(static_cast<u16>(r));
        m.probe(r, sin(-a).raw == -sin(a).raw);
      }
      CHECK(m.checked == 65536);
      CHECK(m.failures == 0);
    }

    SUBCASE("half turn negates sin") {
      mismatch m{};
      for (i32 r = 0; r < 65536; ++r) {
        const angle a = angle::from_raw(static_cast<u16>(r));
        m.probe(r, sin(a + angle::HALF).raw == -sin(a).raw);
      }
      CHECK(m.checked == 65536);
      CHECK(m.failures == 0);
    }

    SUBCASE("cos is exactly even") {
      mismatch m{};
      for (i32 r = 0; r < 65536; ++r) {
        const angle a = angle::from_raw(static_cast<u16>(r));
        m.probe(r, cos(-a).raw == cos(a).raw);
      }
      CHECK(m.checked == 65536);
      CHECK(m.failures == 0);
    }
  }

  TEST_CASE("angle: atan2 round trip") {
    // The strongest self-consistency check available: it exercises the sine
    // table, the interpolation, both CORDIC modes and every fold, and it needs
    // no external reference to be wrong against.
    u64 checked  = 0;
    int worst    = 0;
    i32 worst_at = 0;
    for (i32 r = 0; r < 65536; ++r) {
      ++checked;
      const angle a = angle::from_raw(static_cast<u16>(r));
      const angle b = atan2(sin(a), cos(a));
      const int   d = shortest_delta(a, b);
      const int   e = d < 0 ? -d : d;
      if (e > worst) {
        worst    = e;
        worst_at = r;
      }
    }
    CHECK(checked == 65536);
    INFO("worst round trip ", worst, " BAM units at raw ", worst_at);
    CHECK(worst <= 1);
  }

  TEST_CASE("angle: atan2 on the axes") {
    // The four directions gameplay actually names, and they are EXACT rather
    // than within a tolerance. CORDIC cannot land on an axis -- every step
    // rotates -- so atan2 special-cases them. Without that, clicking straight
    // right returns one unit shy of ZERO and the sin/cos round trip drifts.
    const fx zero = fx::from_int(0);
    const fx one  = fx::from_int(1);

    CHECK(atan2(zero, one) == angle::ZERO);
    CHECK(atan2(one, zero) == angle::QUARTER);
    CHECK(atan2(zero, -one) == angle::HALF);
    CHECK(atan2(-one, zero) == angle::THREE_QUARTER);

    // Magnitude must not matter -- only the ratio.
    CHECK(atan2(zero, fx::from_int(9000)) == angle::ZERO);
    CHECK(atan2(fx::from_raw(1), zero) == angle::QUARTER);
  }

  TEST_CASE("angle: atan2 against double across magnitudes") {
    // Magnitude is the interesting axis here, not direction: atan2 normalises
    // its inputs, and both ends of that are load-bearing. A raw of 1 would be
    // flushed to zero by the shifts inside CORDIC without the shift UP, and a
    // raw near i32's limit would overflow without the shift DOWN.
    const i32 mags[] = { 1, 2, 17, 1000, 65536, 1 << 20, 1 << 28, 1 << 30 };

    deviation d{};
    for (i32 m : mags) {
      for (i32 deg = -179; deg <= 180; ++deg) {
        const double t  = static_cast<double>(deg) * PI / 180.0;
        const i32    px = static_cast<i32>(std::lround(std::cos(t) * m));
        const i32    py = static_cast<i32>(std::lround(std::sin(t) * m));
        if (px == 0 && py == 0) continue;  // atan2(0,0) asserts; see the note

        const angle  got = atan2(fx::from_raw(py), fx::from_raw(px));
        const double want
            = std::atan2(static_cast<double>(py), static_cast<double>(px));
        const double err = wrap_pi(as_radians(got) - want);
        d.probe(m, err, 0.0);
      }
    }

    CHECK(d.checked == 8 * 360);
    const double bam = d.worst / (2.0 * PI) * TURN;
    INFO("worst atan2 ", bam, " BAM units at magnitude ", d.worst_at);
    CHECK(bam <= 1.5);
  }

  TEST_CASE("angle: compile time evaluation") {
    // The sine table is built by CORDIC at consteval time, so if any of this
    // fell out of constant evaluation the table itself would not exist.
    static_assert(sin(angle::QUARTER).raw == 65536);
    static_assert(cos(angle::ZERO).raw == 65536);
    static_assert(sin(angle::HALF).raw == 0);
    static_assert(shortest_delta(angle::ZERO, angle::QUARTER) == 16384);
    static_assert(atan2(fx::from_int(0), fx::from_int(1)) == angle::ZERO);
    CHECK(true);  // the assertions above are the test
  }

  TEST_CASE("angle: formats as degrees with the raw alongside") {
    // Degrees for the human, raw for the evidence. A desync report needs the
    // raw -- it is what is comparable between machines -- but nobody reads
    // 49152 as three quarters of a turn.
    CHECK(std::format("{}", angle::ZERO) == "0 deg angle(0)");
    CHECK(std::format("{}", angle::QUARTER) == "90 deg angle(16384)");
    CHECK(std::format("{}", angle::HALF) == "180 deg angle(32768)");
    CHECK(std::format("{}", angle::THREE_QUARTER) == "270 deg angle(49152)");

    // The inherited parse() means a float spec still applies to the degrees.
    CHECK(std::format("{:.2f}", angle::THREE_QUARTER)
          == "270.00 deg angle(49152)");

    // And this is why the raw half earns its place: from_degrees floors, so
    // one degree is not one degree, and only the raw says by how much.
    CHECK(std::format("{}", angle::from_degrees(1))
          == "0.999755859375 deg angle(182)");
  }

  TEST_CASE("angle: golden hash sequence") {
    // ~100k mixed operations folded into one constant, checked by every CI row
    // -- both architectures, both compilers, every preset -- so cross-platform
    // agreement is enforced by ctest with no artefact comparison anywhere.
    //
    // Same construction as the fx and fx64 hashes. This one covers the sine
    // table, the interpolation, both CORDIC modes and the wrapping arithmetic;
    // a single changed table entry moves it.
    u32  hash = 0x811c9dc5;  // FNV-1a basis
    auto fnv  = [&hash](i32 raw) {
      hash ^= static_cast<u32>(raw);
      hash *= 0x01000193;
    };

    angle a = angle::ZERO;
    for (i32 i = 1; i <= 100000; ++i) {
      a += angle::from_raw(static_cast<u16>(i));
      const fx s = sin(a);
      const fx c = cos(a);
      fnv(s.raw);
      fnv(c.raw);
      if (i % 5 == 0) fnv(static_cast<i32>(atan2(s, c).raw));
      if (i % 11 == 0) fnv(shortest_delta(a, angle::HALF));
    }

    CHECK(hash == 0x8388EF8F);
  }
}
