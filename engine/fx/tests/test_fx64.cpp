#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/fx64.hpp>

// main() comes from tests/doctest_main.cpp.

// do not define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here.

// covers <moba/fx/fx64.hpp>.

// The first half mirrors test_fx.cpp case for case, so a reader who knows one
// file knows the other and a property that holds for fx is visibly asserted
// for fx64 too. Three deliberate divergences, each noted where it occurs:
//
//   1. from_int is swept over samples, not exhaustively. fx's whole-number
//      range is 65536 values; fx64's is all of i32.
//   2. floor/ceil/round/frac/lerp have no fx64 test because they have no fx64
//      implementation -- see the FREE FUNCTIONS note in fx64.hpp.
//   3. there is no _fx64 literal, so the literal group has no counterpart.
//
// The second half is surface fx does not have: widen/narrow/mul_wide, the
// mixed fx operators, and the four-path rounding agreement.

namespace {

// std::abs picks a different overload for i64 depending on whether int64_t is
// `long` (Linux LP64) or `long long` (Darwin). Sidestep it -- the whole file is
// about arithmetic being the same everywhere.
constexpr moba::i64 iabs(moba::i64 v) noexcept { return v < 0 ? -v : v; }

}  // namespace

using namespace moba;

TEST_SUITE("fx/fx64") {
  TEST_CASE("fx64: one times one is one") {
    CHECK(fx64::from_int(1) * fx64::from_int(1) == fx64::from_int(1));
  }

  TEST_CASE("fx64: round trip") {
    SUBCASE("from_int -> floor/trunc/round") {
      // sampled, not exhaustive: from_int is total over i32, so the boundaries
      // and a stride through both signs are what there is to check.
      for (i64 n = I32_MIN; n <= I32_MAX; n += 7919) {
        const fx64 val = fx64::from_int(static_cast<i32>(n));
        CHECK(val.floor_to_int() == n);
        CHECK(val.trunc_to_int() == n);
        CHECK(val.round_to_int() == n);
      }
      // The two ends, which the stride above steps over.
      CHECK(fx64::from_int(I32_MIN).floor_to_int() == I32_MIN);
      CHECK(fx64::from_int(I32_MAX).floor_to_int() == I32_MAX);
      // I32_MIN * 2^32 is exactly I64_MIN, which is why from_int needs no
      // range assert where fx::from_int does.
      CHECK(fx64::from_int(I32_MIN).raw == I64_MIN);
    }

    SUBCASE("from_raw exact points") {
      CHECK(fx64::from_raw(0).trunc_to_int() == 0);
      CHECK(fx64::from_raw(fx64::SCALE).trunc_to_int() == 1);
      CHECK(fx64::from_raw(-fx64::SCALE).trunc_to_int() == -1);
      CHECK(fx64::from_raw(I64_MAX).trunc_to_int() == I32_MAX);
      CHECK(fx64::from_raw(I64_MIN).trunc_to_int() == I32_MIN);
    }
  }

  TEST_CASE("fx64: rounding (floor vs trunc)") {
    const fx64 a = fx64::from_int(-3);
    const fx64 b = fx64::widen(fx::from_ratio(1, 2));

    const fx64 c = a * b;

    CHECK(c.raw == (-2 * fx64::SCALE) + (fx64::SCALE / 2));
    CHECK(c.floor_to_int() == -2);
    CHECK(c.trunc_to_int() == -1);
    CHECK(c.round_to_int() == -1);  // halves round up (-1.5 + 0.5 = -1)

    const fx64 div_res = a / 2;
    CHECK(div_res.floor_to_int() == -2);
    CHECK(div_res == c);
  }

  TEST_CASE("fx64: compound operators") {
    SUBCASE("+= -= *= /= (fx64 operand)") {
      const fx64 a = fx64::from_int(5);
      const fx64 b = fx64::from_int(3);

      // clang-format off
      fx64 c = a; c += b; CHECK(c == a + b);
           c = a; c -= b; CHECK(c == a - b);
           c = a; c *= b; CHECK(c == a * b);
           c = a; c /= b; CHECK(c == a / b);
      // clang-format on
    }

    SUBCASE("+= -= *= (fx operand)") {
      // fx64 has no operator/(fx) by design -- see the TODO in fx64.hpp.
      const fx64 a = fx64::from_int(5);
      const fx   b = fx::from_ratio(3, 4);

      // clang-format off
      fx64 c = a; c += b; CHECK(c == a + b);
           c = a; c -= b; CHECK(c == a - b);
           c = a; c *= b; CHECK(c == a * b);
      // clang-format on
    }

    SUBCASE("*= and /= by a plain integer") {
      const fx64 a = fx64::from_int(7);

      // clang-format off
      fx64 c = a; c *= 3; CHECK(c == a * 3);
           c = a; c /= 3; CHECK(c == a / 3);
      // clang-format on
    }

    SUBCASE("2 *= 3 bug") {
      // The fx analogue: two raw values whose product underflows to zero. It
      // is invisible without a test precisely because the answer looks fine.
      const fx64 a = fx64::from_raw(2);
      const fx64 b = fx64::from_raw(3);

      fx64 c = a;
      c *= b;
      CHECK(c == a * b);
      CHECK(c.raw == 0);
    }
  }

  TEST_CASE("fx64: division") {
    SUBCASE(
        "divide by zero clamping (release behavior assumes clamp, aborts in "
        "debug)"
    ) {
// NOTE: in debug, this aborts due to MOBA_ASSERT. run this specific block
// under Release (NDEBUG) to verify clamping.
#if defined(NDEBUG)
      const fx64 pos  = fx64::from_int(10);
      const fx64 neg  = fx64::from_int(-10);
      const fx64 zero = fx64::ZERO;

      CHECK((pos / zero).raw == I64_MAX);
      CHECK((neg / zero).raw == I64_MIN);
      CHECK((pos / 0).raw == I64_MAX);  // integer overload
      CHECK((neg / 0).raw == I64_MIN);  // integer overload

      // I64_MIN / -1 is the one integer division that overflows. It is
      // intercepted before the divide and returns the value a wrap produces.
      CHECK((fx64::MIN / -1).raw == I64_MIN);
#endif
    }

    SUBCASE("(a / b) * b error scales with |b|") {
      // Same shape as the fx case: dividing loses up to 1 ULP, multiplying
      // back by b SCALES that error by |b|, then operator* floors once more.
      // Tolerance is |b| + 1 ULP, not 1.
      const fx64 a      = fx64::widen(fx::from_raw(800000));
      const fx64 b      = fx64::widen(fx::from_raw(333333));  // ~5.09
      const fx64 result = (a / b) * b;
      const i64  bound  = (iabs(b.raw) >> fx64::SHIFT) + 1;
      CHECK(iabs(result.raw - a.raw) <= bound);
    }

    SUBCASE("(a / b) * b within 1 ULP when |b| <= 1") {
      // The tight bound only holds when b cannot amplify the quotient error.
      const fx64 a      = fx64::widen(fx::from_raw(800000));
      const fx64 b      = fx64::widen(fx::from_ratio(1, 3));
      const fx64 result = (a / b) * b;
      CHECK(iabs(result.raw - a.raw) <= 1);
    }
  }

  TEST_CASE("fx64: algebra properties") {
    const fx64 a = fx64::widen(fx::from_raw(123456));
    const fx64 b = fx64::widen(fx::from_raw(-654321));

    CHECK(a * b == b * a);
    CHECK(a - a == fx64::ZERO);
    CHECK(-(-a) == a);
    // Commutativity of int scaling, both spellings of the free operator.
    CHECK(3 * a == a * 3);
    // and of the mixed-width free operators
    CHECK(fx::from_raw(123456) * b == b * fx::from_raw(123456));
    CHECK(fx::from_raw(123456) + b == b + fx::from_raw(123456));

    // NOT associativity: (a*b)*c != a*(b*c) is expected in fixed point,
    // because each multiply floors. Asserting it would be asserting a bug.
  }

  TEST_CASE("fx64: ordering") {
    CHECK(fx64::from_int(-5) < fx64::from_int(2));
    CHECK(fx64::from_int(2) > fx64::from_int(-5));
    CHECK(fx64::from_int(10) == fx64::from_int(10));
    CHECK(fx64::from_int(0) >= fx64::from_raw(-1));
    // across the sign boundary at the storage limits
    CHECK(fx64::MIN < fx64::ZERO);
    CHECK(fx64::ZERO < fx64::MAX);
  }

  TEST_CASE("fx64: constexpr static assertions") {
    // tests that constexpr isn't just a label; it actually works in
    // compile-time evaluation
    static_assert((fx64::from_int(2) * fx64::from_int(3)).trunc_to_int() == 6);
    static_assert((fx64::from_int(10) / fx64::from_int(2)).trunc_to_int() == 5);
    static_assert((fx64::from_int(5) + fx64::from_int(5)).trunc_to_int() == 10);
    static_assert(
        (fx64::from_int(5) - fx64::from_int(10)).trunc_to_int() == -5
    );
    static_assert(fx64::widen(fx::ONE) == fx64::ONE);
    static_assert(narrow(fx64::ONE) == fx::ONE);
    static_assert(mul_wide(fx::ONE, fx::ONE) == fx64::ONE);
  }

  TEST_CASE("fx64: vs double (sanity correctness)") {
    // loose tolerance, catching gross shift/sign errors.
    // not a determinism test. do not tighten tolerance here.
    //
    // Operands are kept small on purpose: fx64's raw is i64 and a double
    // carries a 53-bit mantissa, so above |raw| 2^53 the reference itself is
    // rounded. See the fx64 note in format.hpp.
    auto test_pair = [](i32 r1, i32 r2) {
      const fx64   a  = fx64::widen(fx::from_raw(r1));
      const fx64   b  = fx64::widen(fx::from_raw(r2));
      const double da = r1 / 65536.0;
      const double db = r2 / 65536.0;

      const fx64   c  = a * b;
      const double dc = da * db;

      CHECK(std::abs(to_double_lossy(c) - dc) < 0.0001);
    };

    test_pair(100000, 50000);
    test_pair(-100000, 50000);
    test_pair(-80000, -80000);
  }

  TEST_CASE("fx64: golden hash sequence") {
    // The fx golden sequence run at Q32.32. Same operand stream, same
    // operations, so the two hashes cover the same dynamics at both widths.
    // Must be identical across every preset and every architecture; the whole
    // point of the constant is that it is committed once and checked by all of
    // them. FNV-1a, 64-bit, because the raws are i64.
    u64  hash = 0xcbf29ce484222325ULL;  // FNV-1a 64 basis
    auto fnv  = [&hash](i64 raw) {
      hash ^= static_cast<u64>(raw);
      hash *= 0x100000001b3ULL;
    };

    fx64 state = fx64::from_int(1);
    for (i32 i = 1; i <= 100000; ++i) {
      // deterministic sequence of operations
      state = state * fx64::widen(fx::from_ratio(101, 100))
              + fx64::widen(fx::from_raw(i));
      if (i % 3 == 0) state = state / 2;
      if (i % 7 == 0) state = -state;
      fnv(state.raw);
    }

    CHECK(hash == 0x99FF089FBBD87198ULL);
  }

  /* ------------------------------------------------------------------
   * Surface fx does not have. Everything below this line is fx64-only.
   * ------------------------------------------------------------------ */

  TEST_CASE("fx64: widen and narrow") {
    SUBCASE("widen is exact and total") {
      // |raw| << 16 maxes at 2^47, so no input can overflow.
      static_assert(fx64::widen(fx::MIN).raw == i64{ I32_MIN } << 16);
      static_assert(fx64::widen(fx::MAX).raw == i64{ I32_MAX } << 16);
      CHECK(fx64::widen(fx::ZERO) == fx64::ZERO);
      CHECK(fx64::widen(fx::ONE) == fx64::ONE);
      CHECK(fx64::widen(fx::EPSILON).raw == i64{ 1 } << 16);
    }

    SUBCASE("narrow(widen(v)) == v for every fx") {
      // widen shifts up 16, narrow shifts down 16, so the round trip is exact
      // by construction. Swept rather than argued.
      for (i64 r = I32_MIN; r <= I32_MAX; r += 65413) {
        const fx v = fx::from_raw(static_cast<i32>(r));
        CHECK(narrow(fx64::widen(v)) == v);
      }
      CHECK(narrow(fx64::widen(fx::MIN)) == fx::MIN);
      CHECK(narrow(fx64::widen(fx::MAX)) == fx::MAX);
    }

    SUBCASE("narrow floors, it does not truncate toward zero") {
      // -1 ULP of fx64 is a negative value whose fx magnitude is under one
      // ULP. Flooring sends it to -1; truncating would send it to 0.
      CHECK(narrow(fx64::from_raw(-1)) == fx::from_raw(-1));
      CHECK(narrow(fx64::from_raw(-fx::SCALE / 2)) == fx::from_raw(-1));
      CHECK(narrow(fx64::from_raw(fx::SCALE / 2)) == fx::from_raw(0));
    }

    SUBCASE("narrow wraps, narrow_sat clamps") {
      // The documented contrast. narrow() wrapping is what makes it agree
      // with fx::operator* on out-of-range products; narrow_sat is the opt-in
      // edge-preserving form. narrow() asserts in debug, so only narrow_sat
      // can be exercised in both arms.
      CHECK(narrow_sat(fx64::MAX) == fx::MAX);
      CHECK(narrow_sat(fx64::MIN) == fx::MIN);
      CHECK(narrow_sat(fx64::widen(fx::ONE)) == fx::ONE);

#if defined(NDEBUG)
      // one ULP past what fx can hold, in raw terms
      const fx64 over = fx64::from_raw((i64{ I32_MAX } + 1) << 16);
      CHECK(narrow(over).raw == I32_MIN);      // wraps
      CHECK(narrow_sat(over).raw == I32_MAX);  // clamps
#endif
    }
  }

  TEST_CASE("fx64: mul_wide is exact") {
    // Q16.16 * Q16.16 is Q32.32 with no shift and no loss. This is the one
    // multiply in the library that throws nothing away, which is why the
    // four-path test below uses it as the reference.
    static_assert(mul_wide(fx::ONE, fx::ONE) == fx64::ONE);
    static_assert(mul_wide(fx::MIN, fx::MIN).raw == i64{ I32_MIN } * I32_MIN);
    static_assert(mul_wide(fx::MAX, fx::MIN).raw == i64{ I32_MAX } * I32_MIN);

    for (i32 ai = -32; ai <= 32; ++ai) {
      for (i32 bi = -32; bi <= 32; ++bi) {
        const fx a = fx::from_raw((ai * 1013) + 7);
        const fx b = fx::from_raw((bi * 7919) - 3);
        // exactness: the raw product, with no rounding anywhere
        CHECK(mul_wide(a, b).raw == i64{ a.raw } * b.raw);
      }
    }
  }

  TEST_CASE("fx64: four-path rounding agreement") {
    // THE test this file exists for. There are four spellings of "multiply
    // two Q16.16 values and store the Q16.16 result":
    //
    //     a * b                          fx::operator*, open-coded
    //     narrow(mul_wide(a, b))         exact product, one narrowing
    //     narrow(widen(a) * widen(b))    fx64::operator*(fx64)
    //     narrow(widen(a) * b)           fx64::operator*(fx)
    //
    // Four paths is six chances to disagree, and a 1-ULP disagreement between
    // any two of them is a desync the moment a golden hash is recorded through
    // one spelling and replayed through another. They agree today because
    // every one of them floors -- arithmetic right shift IS a floor -- and
    // this is what holds them there.
    //
    // Operands are chosen so the Q16.16 product stays in range: the raws below
    // reach ~5.1e5 and ~6.6e4, whose product is ~3.4e10, and >> 16 leaves
    // ~5.1e5. Out-of-range products are the wrapping case, and they abort
    // under the debug asserts rather than round; that contrast belongs to the
    // narrow/narrow_sat case above, not here.
    auto agree = [](fx a, fx b) {
      const fx direct = a * b;
      CHECK(narrow(mul_wide(a, b)) == direct);
      CHECK(narrow(fx64::widen(a) * fx64::widen(b)) == direct);
      CHECK(narrow(fx64::widen(a) * b) == direct);
    };

    SUBCASE("swept over both signs with inexact products") {
      // The strides are coprime with SCALE, so essentially every product has a
      // fractional part -- which is the only case where floor and truncate can
      // differ. A positive-only or exact-only sweep proves nothing.
      for (i32 ai = -64; ai <= 64; ++ai) {
        for (i32 bi = -64; bi <= 64; ++bi) {
          agree(fx::from_raw((ai * 7919) - 3), fx::from_raw((bi * 1013) + 7));
        }
      }
    }

    SUBCASE("named edge cases") {
      agree(fx::ZERO, fx::ZERO);
      agree(fx::ONE, fx::ZERO);
      agree(fx::ONE, -fx::ONE);
      agree(fx::EPSILON, fx::EPSILON);
      agree(-fx::EPSILON, fx::EPSILON);
      agree(-fx::EPSILON, -fx::EPSILON);
      agree(fx::from_ratio(1, 2), -fx::from_int(3));
      agree(fx::from_ratio(-1, 3), fx::from_ratio(1, 7));
    }

    SUBCASE("the direction is floor, not truncate toward zero") {
      // A case where the two rules give different answers, so this fails
      // loudly if anyone reintroduces truncation on any of the four paths.
      const fx a = fx::from_int(-3);
      const fx b = fx::from_ratio(1, 2);
      CHECK((a * b).raw == (-2 * fx::SCALE) + (fx::SCALE / 2));  // -1.5 exactly

      // -1 ULP times half a ULP: the exact product is a negative value smaller
      // than one ULP, so floor gives -1 and truncation would give 0.
      const fx tiny = fx::from_raw(-1) * fx::from_ratio(1, 2);
      CHECK(tiny.raw == -1);
      agree(fx::from_raw(-1), fx::from_ratio(1, 2));
    }
  }

  TEST_CASE("fx64: mixing with fx") {
    const fx64 a = fx64::from_int(10);
    const fx   b = fx::from_ratio(1, 4);

    SUBCASE("+ and - widen the fx operand") {
      CHECK(a + b == a + fx64::widen(b));
      CHECK(a - b == a - fx64::widen(b));
    }

    SUBCASE("operator*(fx) matches going through widen") {
      // The reason operator*(fx) exists is to shift once instead of twice.
      // It must land on the same value as the long way round.
      CHECK(a * b == a * fx64::widen(b));
      for (i32 i = -64; i <= 64; ++i) {
        const fx64 acc = fx64::from_raw(i64{ i } * 1'000'000'007);
        const fx   f   = fx::from_raw((i * 7919) - 3);
        CHECK(acc * f == acc * fx64::widen(f));
      }
    }

    SUBCASE("free operators commute") {
      CHECK(b * a == a * b);
      CHECK(b + a == a + b);
      // There is deliberately no operator-(fx, fx64) and no operator/(fx):
      // subtraction does not commute so the reversed spelling would need its
      // own definition, and division by an fx has no caller yet. See the TODO
      // in fx64.hpp.
    }
  }

  TEST_CASE("fx64: free functions") {
    // The same five as fx. floor/ceil/round/frac/lerp are deliberately absent
    // from fx64 -- it is scratch space, and nothing rounds a value that is
    // about to be narrowed anyway. See the FREE FUNCTIONS note in fx64.hpp.
    SUBCASE("abs") {
      CHECK(abs(fx64::from_int(-5)) == fx64::from_int(5));
      CHECK(abs(fx64::from_int(5)) == fx64::from_int(5));
      CHECK(abs(fx64::ZERO) == fx64::ZERO);
      CHECK(abs(fx64::from_raw(-1)) == fx64::from_raw(1));
      // abs(fx64::MIN) is not representable and asserts; not exercised here.
    }

    SUBCASE("min and max") {
      const fx64 lo = fx64::from_int(-5);
      const fx64 hi = fx64::from_int(2);
      CHECK(min(lo, hi) == lo);
      CHECK(min(hi, lo) == lo);
      CHECK(max(lo, hi) == hi);
      CHECK(max(hi, lo) == hi);
      CHECK(min(fx64::MIN, fx64::MAX) == fx64::MIN);
      CHECK(max(fx64::MIN, fx64::MAX) == fx64::MAX);
    }

    SUBCASE("clamp") {
      const fx64 lo = fx64::from_int(-1);
      const fx64 hi = fx64::from_int(1);
      CHECK(clamp(fx64::from_int(5), lo, hi) == hi);
      CHECK(clamp(fx64::from_int(-5), lo, hi) == lo);
      CHECK(clamp(fx64::ZERO, lo, hi) == fx64::ZERO);
      // boundaries are inclusive
      CHECK(clamp(lo, lo, hi) == lo);
      CHECK(clamp(hi, lo, hi) == hi);
    }

    SUBCASE("sign") {
      CHECK(sign(fx64::from_int(3)) == 1);
      CHECK(sign(fx64::from_int(-3)) == -1);
      CHECK(sign(fx64::ZERO) == 0);
      CHECK(sign(fx64::from_raw(1)) == 1);
      CHECK(sign(fx64::from_raw(-1)) == -1);
    }
  }

  TEST_CASE("fx64: saturating arithmetic") {
    // Opt-in and named. These do NOT assert: clamping is the requested
    // behaviour here, not a bug being reported, so both build arms run the
    // same code and the overflow cases are exercised in debug too.
    SUBCASE("add_sat and sub_sat") {
      CHECK(add_sat(fx64::MAX, fx64::ONE) == fx64::MAX);
      CHECK(add_sat(fx64::MIN, -fx64::ONE) == fx64::MIN);
      CHECK(sub_sat(fx64::MIN, fx64::ONE) == fx64::MIN);
      CHECK(sub_sat(fx64::MAX, -fx64::ONE) == fx64::MAX);
      // in range, they are plain arithmetic
      CHECK(add_sat(fx64::from_int(2), fx64::from_int(3)) == fx64::from_int(5));
      CHECK(
          sub_sat(fx64::from_int(2), fx64::from_int(3)) == fx64::from_int(-1)
      );
    }

    SUBCASE("mul_sat") {
      CHECK(mul_sat(fx64::MAX, fx64::from_int(2)) == fx64::MAX);
      CHECK(mul_sat(fx64::MIN, fx64::from_int(2)) == fx64::MIN);
      CHECK(mul_sat(fx64::MAX, -fx64::ONE) == fx64::from_raw(-I64_MAX));
      CHECK(
          mul_sat(fx64::from_int(3), fx64::from_int(4)) == fx64::from_int(12)
      );
    }

    SUBCASE("div_sat") {
      // Division by zero clamps on the numerator's sign in BOTH arms here --
      // unlike operator/, which asserts first.
      CHECK(div_sat(fx64::from_int(10), fx64::ZERO) == fx64::MAX);
      CHECK(div_sat(fx64::from_int(-10), fx64::ZERO) == fx64::MIN);
      CHECK(div_sat(fx64::ZERO, fx64::ZERO) == fx64::MAX);  // 0 counts as >= 0
      // a quotient too large for the storage
      CHECK(div_sat(fx64::MAX, fx64::from_raw(1)) == fx64::MAX);
      CHECK(div_sat(fx64::MIN, fx64::from_raw(1)) == fx64::MIN);
      // in range
      CHECK(
          div_sat(fx64::from_int(12), fx64::from_int(4)) == fx64::from_int(3)
      );
    }

    SUBCASE("saturating agrees with plain arithmetic in range") {
      // Whatever the _sat forms do at the edges, inside the range they must be
      // the same function -- otherwise swapping one for the other is a desync.
      for (i32 i = -64; i <= 64; ++i) {
        const fx64 a = fx64::from_raw(i64{ i } * 1'000'000'007);
        const fx64 b = fx64::from_raw((i64{ i } * -7919) + 13);
        CHECK(add_sat(a, b) == a + b);
        CHECK(sub_sat(a, b) == a - b);
        CHECK(mul_sat(a, b) == a * b);
        if (b.raw != 0) CHECK(div_sat(a, b) == a / b);
      }
    }
  }

  TEST_CASE("fx64: byte representation") {
    // The static_asserts in fx64.hpp already fail the build if any of this
    // breaks. Restated as a runtime case so ctest names the property that the
    // rollback snapshot and the desync fingerprint both depend on.
    static_assert(sizeof(fx64) == 8);
    static_assert(alignof(fx64) == 8);
    static_assert(std::is_trivially_copyable_v<fx64>);
    static_assert(std::is_standard_layout_v<fx64>);
    static_assert(std::has_unique_object_representations_v<fx64>);
    static_assert(!std::is_aggregate_v<fx64>);  // pins `fx64 a{ 4 }` failing
    CHECK(sizeof(fx64) == 8);
  }
}
