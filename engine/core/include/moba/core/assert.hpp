#pragma once

// moba/core/assert.hpp -- MOBA_ASSERT / MOBA_ASSERT_MSG

// Both macros are defined in both arms and both test cond. cond is evaluated
// exactly once in debug and zero times under NDEBUG. Dangling-else safe, usable
// in a constexpr function, clean under the full warning set in both arms.

// _MSG is the primitive and MOBA_ASSERT defers to it, so the two cannot drift
// apart -- that is how _MSG lost its condition check the first time. It passes
// "" rather than nullptr, which a %s conversion would treat as UB.

// Four things here are important and look like cruft. Do not "simplify" them:

//   1. The NDEBUG arm uses `(void)(false && (cond))`, NOT `(void)sizeof(cond)`.
//      sizeof does not odr-use, so a static function called only inside an
//      assertion is never emitted and -Wunneeded-internal-declaration fails the
//      release build:
//          static bool invariant() { return true; }
//          MOBA_ASSERT(invariant());   // NDEBUG -> -Werror
//      Variables are fine (sizeof suppresses -Wunused-variable); functions are
//      not, and `static bool invariant_holds()` used only in asserts is a shape
//      this codebase will grow. `false && (cond)` is potentially-evaluated, so
//      the function is emitted and the optimiser drops the branch. It still
//      forces contextual conversion to bool.
//
//   2. assertion_failed is `inline`. moba_core is an INTERFACE (header-only)
//      library, so there is no .cpp to define it in.
//
//   3. assertion_failed is deliberately NOT constexpr. That is what produces
//          note: non-constexpr function assertion_failed cannot be used in a
//                constant expression
//      on a failing constant evaluation, instead of the useless
//      `read of non-constexpr variable __stderrp`.
//
//   4. <cstdio>/<cstdlib>/<source_location>/<string_view> are included inside
//      the debug arm only. Hoisting them to the top of the file makes every
//      release TU pay for headers it never uses.

// source_location beats __FILE__/__func__ on both counts: it names the macro-s
// USE site, and function_name() gives the full signature (`int checked(int)`)
// where __func__ gives bare `checked`. -fmacro-prefix-map in MobaWarnings.cmake
// strips the source root from both __FILE__ and source_location::file_name(),
// so no home directory leaks.

#if defined(NDEBUG)
#  define MOBA_ASSERT_MSG(cond, msg) \
    do {                             \
      (void)(false && (cond));       \
      (void)sizeof(msg);             \
    } while (false)
#  define MOBA_ASSERT(cond) MOBA_ASSERT_MSG(cond, "")

#else
#  include <cstdio>
#  include <cstdlib>
#  include <source_location>

namespace moba::detail {
// const char*, not std::string_view: <string_view> is ~48000 preprocessed
// lines and this header reaches every TU in the project. The macros only ever
// pass a stringified condition and a literal, so the view bought nothing --
// and refusing a built std::string on the assert path is the right constraint
// for a sim anyway.
[[noreturn]] inline void assertion_failed(
    const char* cond,
    const char* msg,
    std::source_location loc = std::source_location::current()
) {
  std::fprintf(
      stderr,
      "assert failed at %s:%u in %s: %s: %s\n",
      loc.file_name(),
      loc.line(),
      loc.function_name(),
      cond,
      msg
  );
  std::abort();
}
}  // namespace moba::detail

#  define MOBA_ASSERT_MSG(cond, msg)                     \
    do {                                                 \
      if (cond) [[likely]] {                             \
      } else moba::detail::assertion_failed(#cond, msg); \
    } while (false)

#  define MOBA_ASSERT(cond) MOBA_ASSERT_MSG(cond, "")
#endif

// Tests live in engine/core/tests/test_core.cpp: both macros defined, true
// condition does not abort, dangling-else safe, evaluation count (once in
// debug, zero under NDEBUG), constexpr usability, and no -Wunused-variable
// under NDEBUG. Both arms are exercised because the debug and release presets
// build the same file.

// TODO: [missing] Death test -- "does a failing assert actually abort". Needs a
//       tiny separate executable driven from CMake with a ctest WILL_FAIL
//       property; it cannot live in the doctest binary because it aborts.
