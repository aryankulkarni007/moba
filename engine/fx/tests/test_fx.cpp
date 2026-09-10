#include <cstdlib>
#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/fx.hpp>

// main() comes from tests/doctest_main.cpp.

// do not define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here.

using namespace moba;

TEST_SUITE("fx/fx") {
  TEST_CASE("fx: one times one is one") {
    CHECK(
        moba::fx::from_int(1) * moba::fx::from_int(1) == moba::fx::from_int(1)
    );
  }

  TEST_CASE("fx: round trip") {
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

  TEST_CASE("fx: rounding (floor vs trunc)") {
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

  TEST_CASE("fx: compound operators") {
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

  TEST_CASE("fx: division") {
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

  TEST_CASE("fx: algebra properties") {
    fx a = fx::from_raw(123456);
    fx b = fx::from_raw(-654321);

    CHECK(a * b == b * a);
    CHECK(a - a == fx::ZERO);
    CHECK(-(-a) == a);
    // Commutativity of int scaling
    CHECK(3 * a == a * 3);
  }

  TEST_CASE("fx: ordering") {
    CHECK(fx::from_int(-5) < fx::from_int(2));
    CHECK(fx::from_int(2) > fx::from_int(-5));
    CHECK(fx::from_int(10) == fx::from_int(10));
    CHECK(fx::from_int(0) >= fx::from_raw(-1));
  }

  TEST_CASE("fx: constexpr static assertions") {
    // tests that constexpr isn't just a label; it actually works in
    // compile-time evaluation
    static_assert((fx::from_int(2) * fx::from_int(3)).trunc_to_int() == 6);
    static_assert((fx::from_int(10) / fx::from_int(2)).trunc_to_int() == 5);
    static_assert((fx::from_int(5) + fx::from_int(5)).trunc_to_int() == 10);
    static_assert((fx::from_int(5) - fx::from_int(10)).trunc_to_int() == -5);
    static_assert(fx::from_ratio(1, 4).raw == (fx::SCALE / 4));
  }

  TEST_CASE("fx: vs double (sanity correctness)") {
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

  TEST_CASE("fx: golden hash sequence") {
    // ~100k mixed operations folded into one constant. That constant is
    // committed ONCE, and every CI row -- both architectures, both compilers,
    // every preset -- checks this same value, so cross-platform agreement is
    // enforced by ctest with no artefact comparison anywhere.
    //
    // Not MSVC: types.hpp #errors without __int128, which MSVC lacks.
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

  TEST_CASE("fx: free functions") {
    SUBCASE("abs") {
      CHECK(abs(fx::from_int(-5)) == fx::from_int(5));
      CHECK(abs(fx::from_int(5)) == fx::from_int(5));
      CHECK(abs(fx::ZERO) == fx::ZERO);
      CHECK(abs(fx::from_raw(-1)) == fx::from_raw(1));
      // abs(fx::MIN) is not representable and asserts; not exercised here.
    }

    SUBCASE("min and max") {
      const fx lo = fx::from_int(-5);
      const fx hi = fx::from_int(2);
      CHECK(min(lo, hi) == lo);
      CHECK(min(hi, lo) == lo);
      CHECK(max(lo, hi) == hi);
      CHECK(max(hi, lo) == hi);
      CHECK(min(fx::MIN, fx::MAX) == fx::MIN);
      CHECK(max(fx::MIN, fx::MAX) == fx::MAX);
    }

    SUBCASE("clamp") {
      const fx lo = fx::from_int(-1);
      const fx hi = fx::from_int(1);
      CHECK(clamp(fx::from_int(5), lo, hi) == hi);
      CHECK(clamp(fx::from_int(-5), lo, hi) == lo);
      CHECK(clamp(fx::ZERO, lo, hi) == fx::ZERO);
      // boundaries are inclusive
      CHECK(clamp(lo, lo, hi) == lo);
      CHECK(clamp(hi, lo, hi) == hi);
    }

    SUBCASE("sign") {
      CHECK(sign(fx::from_int(3)) == 1);
      CHECK(sign(fx::from_int(-3)) == -1);
      CHECK(sign(fx::ZERO) == 0);
      CHECK(sign(fx::EPSILON) == 1);
      CHECK(sign(fx::from_raw(-1)) == -1);
    }

    SUBCASE("floor, ceil, round -- the negative half is the whole test") {
      // On positives every rounding rule agrees, so only negatives distinguish
      // them. -0.5 is the case where all three answers differ.
      const fx neg_half = fx::from_ratio(-1, 2);
      CHECK(floor(neg_half) == fx::from_int(-1));
      CHECK(ceil(neg_half) == fx::ZERO);
      CHECK(round(neg_half) == fx::ZERO);  // halves upward

      const fx pos_half = fx::from_ratio(1, 2);
      CHECK(floor(pos_half) == fx::ZERO);
      CHECK(ceil(pos_half) == fx::ONE);
      CHECK(round(pos_half) == fx::ONE);

      // exact whole numbers are fixed points of all three
      CHECK(floor(fx::from_int(3)) == fx::from_int(3));
      CHECK(ceil(fx::from_int(3)) == fx::from_int(3));
      CHECK(round(fx::from_int(3)) == fx::from_int(3));
      CHECK(floor(fx::from_int(-3)) == fx::from_int(-3));
      CHECK(ceil(fx::from_int(-3)) == fx::from_int(-3));
      CHECK(round(fx::from_int(-3)) == fx::from_int(-3));

      // floor(v) agrees with the member floor_to_int()
      for (i32 r = I32_MIN + 65413; r < I32_MAX - 65413; r += 65413) {
        const fx v = fx::from_raw(r);
        CHECK(floor(v).floor_to_int() == v.floor_to_int());
      }
    }

    SUBCASE("frac is always in [0, 1), including for negatives") {
      // The identity that pins it: floor(v) + frac(v) == v for EVERY v.
      // frac(-0.25) is 0.75, not -0.25.
      CHECK(frac(fx::from_ratio(-1, 4)) == fx::from_ratio(3, 4));
      CHECK(frac(fx::from_ratio(1, 4)) == fx::from_ratio(1, 4));
      CHECK(frac(fx::from_int(3)) == fx::ZERO);
      CHECK(frac(fx::from_int(-3)) == fx::ZERO);

      for (i32 r = I32_MIN + 65413; r < I32_MAX - 65413; r += 65413) {
        const fx v = fx::from_raw(r);
        CHECK(frac(v).raw >= 0);
        CHECK(frac(v) < fx::ONE);
        CHECK(floor(v) + frac(v) == v);
      }
    }

    SUBCASE("lerp hits both endpoints exactly") {
      // The reason lerp is written a + (b - a) * t and not a*(1-t) + b*t.
      // Only this form gives both endpoints exactly, and that is an exit
      // criterion -- an interpolation that overshoots its own endpoint by a
      // ULP puts an entity past the wall it was moving toward.
      const fx a = fx::from_int(-3);
      const fx b = fx::from_int(5);
      CHECK(lerp(a, b, fx::ZERO) == a);
      CHECK(lerp(a, b, fx::ONE) == b);
      CHECK(lerp(a, b, fx::from_ratio(1, 2)) == fx::from_int(1));
      // degenerate: a == b is constant for every t
      CHECK(lerp(a, a, fx::from_ratio(1, 3)) == a);
    }
  }

  TEST_CASE("fx: saturating arithmetic") {
    // Opt-in and named. These do NOT assert: clamping is the requested
    // behaviour here, not a bug being reported, so both build arms run the
    // same code and the overflow cases are exercised in debug too.
    SUBCASE("add_sat and sub_sat") {
      CHECK(add_sat(fx::MAX, fx::ONE) == fx::MAX);
      CHECK(add_sat(fx::MIN, -fx::ONE) == fx::MIN);
      CHECK(sub_sat(fx::MIN, fx::ONE) == fx::MIN);
      CHECK(sub_sat(fx::MAX, -fx::ONE) == fx::MAX);
      // in range, they are plain arithmetic
      CHECK(add_sat(fx::from_int(2), fx::from_int(3)) == fx::from_int(5));
      CHECK(sub_sat(fx::from_int(2), fx::from_int(3)) == fx::from_int(-1));
    }

    SUBCASE("mul_sat") {
      CHECK(mul_sat(fx::MAX, fx::from_int(2)) == fx::MAX);
      CHECK(mul_sat(fx::MIN, fx::from_int(2)) == fx::MIN);
      CHECK(mul_sat(fx::MAX, -fx::ONE) == fx::from_raw(-I32_MAX));
      CHECK(mul_sat(fx::from_int(3), fx::from_int(4)) == fx::from_int(12));
    }

    SUBCASE("div_sat") {
      // Division by zero clamps on the numerator's sign in BOTH arms here --
      // unlike operator/, which asserts first.
      CHECK(div_sat(fx::from_int(10), fx::ZERO) == fx::MAX);
      CHECK(div_sat(fx::from_int(-10), fx::ZERO) == fx::MIN);
      CHECK(div_sat(fx::ZERO, fx::ZERO) == fx::MAX);  // 0 counts as >= 0
      // a quotient too large for the storage
      CHECK(div_sat(fx::MAX, fx::EPSILON) == fx::MAX);
      CHECK(div_sat(fx::MIN, fx::EPSILON) == fx::MIN);
      // in range
      CHECK(div_sat(fx::from_int(12), fx::from_int(4)) == fx::from_int(3));
    }

    SUBCASE("saturating agrees with plain arithmetic in range") {
      // Whatever the _sat forms do at the edges, inside the range they must be
      // the same function -- otherwise swapping one for the other is a desync.
      for (i32 i = -64; i <= 64; ++i) {
        const fx a = fx::from_raw((i * 7919) - 3);
        const fx b = fx::from_raw((i * 1013) + 7);
        CHECK(add_sat(a, b) == a + b);
        CHECK(sub_sat(a, b) == a - b);
        CHECK(mul_sat(a, b) == a * b);
        if (b.raw != 0) CHECK(div_sat(a, b) == a / b);
      }
    }
  }

  TEST_CASE("fx: byte representation") {
    // The static_asserts in fx.hpp already fail the build if any of this
    // breaks. Restated as a runtime case so ctest names the property that the
    // rollback snapshot and the desync fingerprint both depend on.
    static_assert(sizeof(fx) == 4);
    static_assert(alignof(fx) == 4);
    static_assert(std::is_trivially_copyable_v<fx>);
    static_assert(std::is_standard_layout_v<fx>);
    static_assert(std::has_unique_object_representations_v<fx>);
    static_assert(!std::is_aggregate_v<fx>);  // pins `fx a{ 4 }` failing
    CHECK(sizeof(fx) == 4);
  }

  // clang-format off

  /* THE _fx LITERAL
   *
   * Every one of these is a static_assert rather than a CHECK, because the
   * operator is consteval: if this file compiles, they held. The runtime body
   * exists only so ctest names the group. */

  TEST_CASE("fx: _fx literal: exact against from_ratio") {
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

  TEST_CASE("fx: _fx literal: integer literals") {
    // The entire reason for the raw const char* form. A cooked long double
    // operator cannot match an integer literal at all, so 1_fx did not compile.
    static_assert((0_fx).raw     == 0);
    static_assert((1_fx).raw     == fx::SCALE);
    static_assert((1000_fx).raw  == 1000 * fx::SCALE);
    static_assert((32767_fx).raw == fx::MAX_INT * fx::SCALE);
    static_assert((1_fx).raw     == fx::from_int(1).raw);
  }

  TEST_CASE("fx: _fx literal: spelling does not change the value") {
    static_assert((1.50_fx).raw  == (1.5_fx).raw);
    static_assert((1.000_fx).raw == (1_fx).raw);
    static_assert((00.5_fx).raw  == (0.5_fx).raw);
    static_assert((.5_fx).raw    == (0.5_fx).raw);  // leading point
    static_assert((1._fx).raw    == (1_fx).raw);    // trailing point
    static_assert((0.0_fx).raw   == 0);
  }

  TEST_CASE("fx: _fx literal: digits past FRAC_DIGIT_CAP are dropped") {
    // Pins that the extra digits are consumed and ignored, not rejected.
    static_assert((0.123456789012_fx).raw == (0.123456789_fx).raw);

    // ...and pins the price of that, which is NOT zero. One ULP is
    // 0.0000152587890625 -- sixteen significant digits -- so truncating at
    // nine can land one ULP low. This literal IS exactly fx::EPSILON and
    // parses to zero. Documented, bounded at 1 ULP, and only reachable with
    // more than nine fractional digits; from_ratio is the exact spelling.
    // See THE _fx LITERAL in <moba/fx/fx.hpp>.
    static_assert((0.0000152587890625_fx).raw == 0);
    static_assert(fx::from_ratio(1, 65536).raw == 1);
  }

  TEST_CASE("fx: _fx literal: the sign is applied AFTER the literal") {
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

  TEST_CASE("fx: _fx literal: agrees with arithmetic") {
    // Ties the literal to the operators rather than testing it in isolation.
    static_assert((0.5_fx * 2).raw        == (1_fx).raw);
    static_assert((2_fx * 0.5_fx).raw     == (1_fx).raw);
    static_assert((0.25_fx + 0.25_fx).raw == (0.5_fx).raw);
    static_assert((1.5_fx - 0.5_fx).raw   == (1_fx).raw);
  }

  TEST_CASE("fx: _fx literal: upper boundary") {
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

// What this file does NOT cover, deliberately:
//
//   associativity  (a*b)*c != a*(b*c) is EXPECTED in fixed point -- each
//                  multiply floors, so the two groupings lose different bits.
//                  Asserting it would be asserting a bug.
//
//   from_int out of range  from_int(32768) wraps silently in release and
//                  asserts in debug, so there is no single answer to check.
//                  The _fx literal is the spelling that rejects out-of-range
//                  values at compile time; that is what compile_fail/ pins.
//
//   fx64 interop   lives in test_fx64.cpp, together with the four-path
//                  rounding agreement between fx::operator*, mul_wide/narrow
//                  and the two fx64 multiplies.
