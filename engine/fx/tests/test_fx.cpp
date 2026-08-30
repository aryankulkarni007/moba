#include <doctest/doctest.h>
#include <moba/fx/format.hpp>
#include <moba/fx/fx.hpp>

// main() comes from tests/doctest_main.cpp. Do not define
// DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here.

TEST_CASE("one times one is one") {
  CHECK(moba::fx::from_int(1) * moba::fx::from_int(1) == moba::fx::from_int(1));
}

// TODO: The real suite. What each test must prove, not how to write it.
//       Several will FAIL against current code -- that is the point. Write the
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
//                  today for *= and /=. Include 2 *= 3 specifically -- it
//                  gives raw 0, which is why it is invisible without a test.
//
//   division       div by zero: positive numerator -> INT32_MAX, negative ->
//                  INT32_MIN, zero -> whatever you decide. Debug aborts
//                  instead; test one configuration and say which.
//                  INT32_MIN / -1. (a / b) * b within a named ULP tolerance.
//
//   algebra        a * b == b * a exactly, including negatives. Identities,
//                  a - a == 0, -(-a) == a.
//                  NOT associativity -- (a*b)*c != a*(b*c) in fixed point is
//                  expected. If tempted, leave a comment saying why not.
//
//   ordering       <=> agrees with raw ordering across the sign boundary.
//
//   constexpr      static_assert block exercising each operator. Finds
//                  operations that are constexpr in name only. Free.
//
//   vs double      Loose tolerance, few thousand sampled pairs. CORRECTNESS
//                  test, not determinism -- it catches gross sign/shift errors
//                  only. Say so in a comment so nobody tightens it later into
//                  a flaky cross-platform failure.
//
//   golden hash    ~100k mixed ops over a fixed operand sequence including
//                  negatives, zero and boundaries, folded into one hash.
//                  Sequence generated from a fixed seed (reproducible, no data
//                  file). Expected hash is one named constant. Must be
//                  identical under debug, release and gcc-release.
//                  Until gcc-release runs on real GCC it is clang vs clang.
