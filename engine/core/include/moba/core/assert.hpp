#pragma once

// moba/core/assert.hpp -- MOBA_ASSERT / MOBA_ASSERT_MSG
//
// Verified working: both macros defined in both arms; both test cond; cond
// evaluated exactly once; dangling-else safe; usable in a constexpr function
// (static_assert on a passing call compiles); compiles clean under the full
// warning set in debug. [[unlikely]] and __func__ are both good additions --
// __func__ works in constructors and static functions.
//
// __FILE__ is already repo-relative: -fmacro-prefix-map in MobaWarnings.cmake
// strips the source root, so output leaks no home directory.
//
// TODO: [BUG] `(void)msg` EVALUATES msg. It is an expression statement, not an
//       unevaluated operand like sizeof. So a msg with side effects runs in
//       release and not in debug -- backwards from every expectation:
//           MOBA_ASSERT_MSG(1 == 1, side_effect());
//               debug  -> side_effect() called 0 times
//               NDEBUG -> side_effect() called 1 time
//       Fix: `(void)sizeof(msg)`, matching the cond line directly above it.
//
// TODO: [BUG] A static (internal-linkage) function used only inside an
//       assertion fails the release build:
//           static bool invariant() { return true; }
//           MOBA_ASSERT(invariant());
//           NDEBUG -> error: function 'invariant' is not needed and will not
//                     be emitted [-Werror,-Wunneeded-internal-declaration]
//       sizeof does not odr-use, so the function is never emitted and -Wall
//       objects. Variables are fine -- sizeof suppresses -Wunused-variable --
//       but functions are not, and `static bool invariant_holds()` used only
//       in asserts is a shape this codebase will grow.
//       Fix: `(void)(false && (cond))`. Potentially-evaluated, so the function
//       is emitted; the optimiser drops the branch. Verified it fixes the
//       case and still forces contextual conversion to bool.
//
// TODO: [duplication] Four near-identical bodies now. Make MOBA_ASSERT_MSG the
//       primitive in each arm and define MOBA_ASSERT(cond) in terms of it.
//       This is how _MSG lost its condition check the first time.
//
//       Careful: `_MSG(cond, nullptr)` would pass nullptr to a %s conversion,
//       which is UB. If the failure path becomes an out-of-line function (see
//       below) it can branch on msg != nullptr; if it stays a macro, pass ""
//       rather than nullptr.
//
// TODO: [ergonomics] constexpr use works, but a failing assert during constant
//       evaluation reports:
//           note: read of non-constexpr variable '__stderrp' is not allowed
//       Route the failure through one named [[noreturn]] detail function:
//           note: non-constexpr function 'assertion_failed' cannot be used
//       Also fixes the include asymmetry: <cstdio>/<cstdlib> are pulled into
//       every debug TU and none in release, from a header this widely
//       included.

#if defined(NDEBUG)

#define MOBA_ASSERT(cond)                                                                          \
  do {                                                                                             \
    (void)sizeof(cond);                                                                            \
  } while (false)

#define MOBA_ASSERT_MSG(cond, msg)                                                                 \
  do {                                                                                             \
    (void)sizeof(cond);                                                                            \
    (void)msg;                                                                                     \
  } while (false)

#else
#include <cstdio>
#include <cstdlib>

#define MOBA_ASSERT(cond)                                                                          \
  do {                                                                                             \
    if (cond) [[likely]]                                                                           \
      break;                                                                                       \
    std::fprintf(stderr, "assert failed at %s:%d in %s: %s\n", __FILE__, __LINE__, __func__,       \
                 #cond);                                                                           \
    std::abort();                                                                                  \
  } while (false)
#define MOBA_ASSERT_MSG(cond, msg)                                                                 \
  do {                                                                                             \
    if (cond) [[likely]]                                                                           \
      break;                                                                                       \
    std::fprintf(stderr, "assert failed at %s:%d in %s: %s: %s\n", __FILE__, __LINE__, __func__,   \
                 #cond, msg);                                                                      \
    std::abort();                                                                                  \
  } while (false)
#endif

// TODO: [missing] No tests. test_core.cpp must cover:
//         - both macros defined and compiling, in a debug TU AND an NDEBUG TU
//           (the second would have caught the release bug)
//         - a TRUE condition does not abort
//         - usable as `if (x) MOBA_ASSERT(y); else z();`
//         - cond evaluated exactly once (counter expression)
//         - usable in a constexpr function (static_assert on a passing call)
//         - under NDEBUG, a variable used only in an assert does not trip
//           -Wunused-variable
//       "Does a failing assert actually abort" is a death test; drive it from
//       CMake with a tiny executable and a ctest WILL_FAIL property.
