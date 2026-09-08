#include <cstdlib>
#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/fx.hpp>

// main() comes from tests/doctest_main.cpp.

// do not define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here.

using namespace moba;

TEST_SUITE("fx/fx") {
  TEST_CASE("one times one is one") {
    CHECK(
        moba::fx::from_int(1) * moba::fx::from_int(1) == moba::fx::from_int(1)
    );
  }

  TEST_CASE("round trip") {
    SUBCASE("from_int -> floor/trunc/round") {
      for (i32 n = fx::MIN_INT; n <= fx::MAX_INT; ++n) {
        fx val = fx::from_int(n);
        CHECK(val.floor_to_int() == n);
        CHECK(val.trunc_to_int() == n);
        CHECK(val.round_to_int() == n);
      }
    }

    SUBCASE("from_raw exact points") {
      CHECK(fx::from_raw(0).trunc_to_int() == 0);
      CHECK(fx::from_raw(fx::SCALE).trunc_to_int() == 1);
      CHECK(fx::from_raw(-fx::SCALE).trunc_to_int() == -1);
      CHECK(fx::from_raw(I32_MAX).trunc_to_int() == fx::MAX_INT);
      CHECK(fx::from_raw(I32_MIN).trunc_to_int() == fx::MIN_INT);
    }
  }

  TEST_CASE("rounding (floor vs trunc)") {
    fx a = fx::from_int(-3);
    fx b = fx::from_ratio(1, 2);

    fx c = a * b;

    CHECK(c.raw == (-2 * fx::SCALE) + (fx::SCALE / 2));
    CHECK(c.floor_to_int() == -2);
    CHECK(c.trunc_to_int() == -1);
    CHECK(c.round_to_int() == -1);  // halves round up (-1.5 + 0.5 = -1)

    fx div_res = a / 2;
    CHECK(div_res.floor_to_int() == -2);
    CHECK(div_res == c);
  }

  TEST_CASE("compound operators") {
    SUBCASE("+= -= *= /=") {
      fx a = fx::from_int(5);
      fx b = fx::from_int(3);

      // clang-format off
      fx c = a; c += b; CHECK(c == a + b);
         c = a; c -= b; CHECK(c == a - b);
         c = a; c *= b; CHECK(c == a * b);
         c = a; c /= b; CHECK(c == a / b);
      // clang-format on
    }

    SUBCASE("2 *= 3 bug") {
      fx a = fx::from_raw(2);
      fx b = fx::from_raw(3);

      fx c = a;
      c *= b;
      CHECK(c == a * b);
      CHECK(c.raw == 0);
    }
  }

  TEST_CASE("division") {
    SUBCASE(
        "divide by zero clamping (release behavior assumes clamp, aborts in "
        "debug)"
    ) {
// NOTE: in debug, this aborts due to MOBA_ASSERT. run this specific block
// under Release (NDEBUG) to verify clamping.
#if defined(NDEBUG)
      fx pos  = fx::from_int(10);
      fx neg  = fx::from_int(-10);
      fx zero = fx::ZERO;

      CHECK((pos / zero).raw == I32_MAX);
      CHECK((neg / zero).raw == I32_MIN);
      CHECK((pos / 0).raw == I32_MAX);  // integer overload
      CHECK((neg / 0).raw == I32_MIN);  // integer overload
#endif
    }

    SUBCASE("(a / b) * b error scales with |b|") {
      // Dividing loses up to 1 ULP. Multiplying back by b SCALES that error by
      // |b|, then operator* floors once more. So the round-trip tolerance is
      // |b| + 1 ULP, not 1 -- with b ~ 5.09 below, an error of 3 is correct.
      // Verified over 7.1M in-range (a, b) pairs: 0 violations of this bound,
      // worst observed 46 ULP at |b| ~ 45.2.
      fx        a      = fx::from_raw(800000);
      fx        b      = fx::from_raw(333333);  // ~5.09
      fx        result = (a / b) * b;
      const i32 bound  = (std::abs(b.raw) >> fx::SHIFT) + 1;
      CHECK(std::abs(result.raw - a.raw) <= bound);
    }

    SUBCASE("(a / b) * b within 1 ULP when |b| <= 1") {
      // The tight bound only holds when b cannot amplify the quotient error.
      fx a      = fx::from_raw(800000);
      fx b      = fx::from_ratio(1, 3);
      fx result = (a / b) * b;
      CHECK(std::abs(result.raw - a.raw) <= 1);
    }
  }

  TEST_CASE("algebra properties") {
    fx a = fx::from_raw(123456);
    fx b = fx::from_raw(-654321);

    CHECK(a * b == b * a);
    CHECK(a - a == fx::ZERO);
    CHECK(-(-a) == a);
    // Commutativity of int scaling
    CHECK(3 * a == a * 3);
  }

  TEST_CASE("ordering") {
    CHECK(fx::from_int(-5) < fx::from_int(2));
    CHECK(fx::from_int(2) > fx::from_int(-5));
    CHECK(fx::from_int(10) == fx::from_int(10));
    CHECK(fx::from_int(0) >= fx::from_raw(-1));
  }

  TEST_CASE("constexpr static assertions") {
    // tests that constexpr isn't just a label; it actually works in
    // compile-time evaluation
    static_assert((fx::from_int(2) * fx::from_int(3)).trunc_to_int() == 6);
    static_assert((fx::from_int(10) / fx::from_int(2)).trunc_to_int() == 5);
    static_assert((fx::from_int(5) + fx::from_int(5)).trunc_to_int() == 10);
    static_assert((fx::from_int(5) - fx::from_int(10)).trunc_to_int() == -5);
    static_assert(fx::from_ratio(1, 4).raw == (fx::SCALE / 4));
  }

  TEST_CASE("vs double (sanity correctness)") {
    // loose tolerance, catching gross shift/sign errors.
    // not a determinism test. do not tighten tolerance here.
    auto test_pair = [](i32 r1, i32 r2) {
      fx     a  = fx::from_raw(r1);
      fx     b  = fx::from_raw(r2);
      double da = r1 / 65536.0;
      double db = r2 / 65536.0;

      fx     c  = a * b;
      double dc = da * db;

      // Ensure within ~2 units of precision.
      double c_double = c.raw / 65536.0;
      CHECK(std::abs(c_double - dc) < 0.0001);
    };

    test_pair(100000, 50000);
    test_pair(-100000, 50000);
    test_pair(-80000, -80000);
  }

  TEST_CASE("golden hash sequence") {
    // hash state simulating ~100k determinism tests.
    // must be identical across gcc, clang, msvc in release and debug.
    u32  hash = 0x811c9dc5;  // FNV-1a basis
    auto fnv  = [&hash](i32 raw) {
      hash ^= static_cast<u32>(raw);
      hash *= 0x01000193;
    };

    fx state = fx::from_int(1);
    for (i32 i = 1; i <= 100000; ++i) {
      // deterministic sequence of operations
      state = state * fx::from_ratio(101, 100) + fx::from_raw(i);
      if (i % 3 == 0) state = state / 2;
      if (i % 7 == 0) state = -state;
      fnv(state.raw);
    }

    CHECK(hash == 0x6B78D1DC);
  }

  // clang-format off

  /* THE _fx LITERAL
   *
   * Every one of these is a static_assert rather than a CHECK, because the
   * operator is consteval: if this file compiles, they held. The runtime body
   * exists only so ctest names the group. */

  TEST_CASE("_fx literal: exact against from_ratio") {
    // from_ratio reaches the same value by a different route -- fdiv on a
    // ratio rather than digit accumulation -- so this is a real cross-check.
    // The MULTI-DIGIT fractions are the ones that earn their place: a
    // per-digit accumulator bug is invisible at one fractional digit and wrong
    // from two onward, so 0.1 and 1.5 alone would pass a broken parser.
    static_assert((0.1_fx).raw      == fx::from_ratio(1, 10).raw);
    static_assert((0.25_fx).raw     == fx::from_ratio(1, 4).raw);
    static_assert((0.125_fx).raw    == fx::from_ratio(1, 8).raw);
    static_assert((3.14159_fx).raw  == fx::from_ratio(314159, 100000).raw);
    static_assert((0.5_fx).raw      == fx::from_ratio(1, 2).raw);
    static_assert((1.5_fx).raw      == fx::from_ratio(3, 2).raw);
    static_assert((0.999999_fx).raw == fx::from_ratio(999999, 1000000).raw);
  }

  TEST_CASE("_fx literal: integer literals") {
    // The entire reason for the raw const char* form. A cooked long double
    // operator cannot match an integer literal at all, so 1_fx did not compile.
    static_assert((0_fx).raw     == 0);
    static_assert((1_fx).raw     == fx::SCALE);
    static_assert((1000_fx).raw  == 1000 * fx::SCALE);
    static_assert((32767_fx).raw == fx::MAX_INT * fx::SCALE);
    static_assert((1_fx).raw     == fx::from_int(1).raw);
  }

  TEST_CASE("_fx literal: spelling does not change the value") {
    static_assert((1.50_fx).raw  == (1.5_fx).raw);
    static_assert((1.000_fx).raw == (1_fx).raw);
    static_assert((00.5_fx).raw  == (0.5_fx).raw);
    static_assert((.5_fx).raw    == (0.5_fx).raw);  // leading point
    static_assert((1._fx).raw    == (1_fx).raw);    // trailing point
    static_assert((0.0_fx).raw   == 0);
  }

  TEST_CASE("_fx literal: digits past FRAC_DIGIT_CAP are dropped") {
    // One ULP is about 1.5e-5, so the tenth decimal cannot move the result.
    // Pins that the extra digits are consumed and ignored, not rejected.
    static_assert((0.123456789012_fx).raw == (0.123456789_fx).raw);
  }

  TEST_CASE("_fx literal: the sign is applied AFTER the literal") {
    // A literal operator never sees the minus. -0.1_fx is -(0.1_fx), so the
    // MAGNITUDE floors and negation -- which is exact -- happens afterwards.
    static_assert((-0.1_fx).raw == -((0.1_fx).raw));

    // Which means a negative literal sits toward zero relative to a true
    // floor. These inequalities are asserted ON PURPOSE. The asymmetry is
    // structural, not a bug: no rounding rule inside the operator can fix it,
    // because the sign is not part of the literal. Anyone who "fixes" it
    // should fail here and read THE _fx LITERAL in <moba/fx/fx.hpp> before
    // quietly changing every negative constant in the sim.
    static_assert((-0.1_fx).raw != fx::from_ratio(-1, 10).raw);
    static_assert((-0.4_fx).raw != fx::from_ratio(-2, 5).raw);
  }

  TEST_CASE("_fx literal: agrees with arithmetic") {
    // Ties the literal to the operators rather than testing it in isolation.
    static_assert((0.5_fx * 2).raw        == (1_fx).raw);
    static_assert((2_fx * 0.5_fx).raw     == (1_fx).raw);
    static_assert((0.25_fx + 0.25_fx).raw == (0.5_fx).raw);
    static_assert((1.5_fx - 0.5_fx).raw   == (1_fx).raw);
  }

  TEST_CASE("_fx literal: upper boundary") {
    // MAX_INT << SHIFT is 2147418112 and the fraction is always under 65536,
    // so a literal that passes the integer-part check cannot overflow.
    static_assert((32767_fx).raw     == fx::MAX_INT * fx::SCALE);
    static_assert((32767.999_fx).raw <= I32_MAX);

    // Rejections -- 32768_fx, 1e3_fx, 0x10_fx, 1'000_fx -- are COMPILE errors
    // and cannot be asserted here. They live in compile_fail/, wired with
    // moba_add_compile_fail_test() in this directory's CMakeLists.txt.
  }
  // clang-format on
}

// TODO: The real suite. What each test must prove, not how to write it.
//       Several will FAIL against current code - that is the point. Write the
//       test, watch it fail, then fix the matching TODO in fx.hpp.
//
//   round-trip     from_int(n).to_int() == n over the stated range; the test
//                  must state that range. Doubles as the from_int overflow
//                  regression. from_raw round-trips 0, 1, -1, INT32_MIN/MAX.
//
//   rounding       The important one. On NEGATIVE operands whose exact product
//                  has a fraction, assert all three agree:
//                      a * b
//                      narrow(mul_wide(a, b))
//                      narrow(fx64{a} * fx64{b})
//                  Then prove the direction is floor, not trunc-toward-zero,
//                  with a case where they differ. Positive-only operands make
//                  the two identical and prove nothing.
//                  Same for to_int on negatives: to_int(-0.5) == -1.
//
//   compound       `fx c = a; c OP= b;` == `a OP b` for every operator. Fails
//                  today for *= and /=. Include 2 *= 3 specifically - it
//                  gives raw 0, which is why it is invisible without a test.
//
//   division       div by zero: positive numerator -> INT32_MAX, negative ->
//                  INT32_MIN, zero -> whatever you decide. Debug aborts
//                  instead; test one configuration and say which.
//                  INT32_MIN / -1. (a / b) * b within a named ULP tolerance.
//
//   algebra        a * b == b * a exactly, including negatives. Identities,
//                  a - a == 0, -(-a) == a.
//                  NOT associativity - (a*b)*c != a*(b*c) in fixed point is
//                  expected. If tempted, leave a comment saying why not.
//
//   ordering       <=> agrees with raw ordering across the sign boundary.
//
//   constexpr      static_assert block exercising each operator. Finds
//                  operations that are constexpr in name only. Free.
//
//   vs double      Loose tolerance, few thousand sampled pairs. CORRECTNESS
//                  test, not determinism - it catches gross sign/shift errors
//                  only. Say so in a comment so nobody tightens it later into
//                  a flaky cross-platform failure.
//
//   golden hash    ~100k mixed ops over a fixed operand sequence including
//                  negatives, zero and boundaries, folded into one hash.
//                  Sequence generated from a fixed seed (reproducible, no data
//                  file). Expected hash is one named constant. Must be
//                  identical under debug, release and gcc-release.
//                  Until gcc-release runs on real GCC it is clang vs clang.
