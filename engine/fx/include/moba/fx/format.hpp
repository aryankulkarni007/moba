#pragma once

// moba/fx/format.hpp -- text output for the fx types. Tooling only.

// Why std::format and not ostream, all three verified:
//   1. Precision. format's default for double is the shortest representation
//      that round-trips, so it prints the exact value; ostream's default is 6
//      significant figures. On fx raw == 1: format 1.52587890625e-05, ostream
//      1.52588e-05. One ULP is ~1.5e-5, so at ostream's default two values one
//      ULP apart can print identically -- the exact case this header exists to
//      diagnose. Do NOT "fix" it with a fixed precision: {:.6f} gives 0.000015,
//      which is worse.
//   2. Locale. ostream follows the global locale (de_DE.UTF-8 gives "1,5");
//      format is locale-independent unless you ask with {:L}. Output you might
//      diff between two machines must not depend on the reader's locale.
//   3. Format strings are checked at compile time.
//
// Decisions, so they are not relitigated:
//   - to_double lives HERE, not as fx::operator double(). Sim TUs never
//     include this header, so the include graph keeps floating point out of
//     the simulation with nothing to remember and no macro to get wrong.
//   - The formatters inherit std::formatter<double>, so its parse() is
//     inherited and the whole float spec works ({:.4f}, {:>12}, {:+}) with no
//     parse() written here. Cost: no custom spec letters, e.g. no {:r} for
//     raw-only.
//   - The raw is ALWAYS appended: "1.5 fx(98304)". fx64 needs it regardless
//     (see below), and making it conditional would mean hand-writing parse().
//   - operator<< exists only because doctest stringifies through it. It holds
//     no logic; std::format is the single implementation.
//   - <format> is a heavy header. It stays here and never enters fx.hpp.
//
// Verified byte-identical on Clang 17 / libc++ and GCC 16 / libstdc++.

// fx64 note: fx64's decimal is an approximation and its raw is authoritative.
// raw is i64, a double carries a 53-bit mantissa, so any |raw| above 2^53
// prints rounded -- that is fx64 values above ~2^21 (about 2.1 million),
// easily reached by intermediate products from mul_wide, which is precisely
// what the rounding-agreement test inspects. fx is not affected: raw is i32,
// every i32 converts to double exactly, and dividing by 2^16 only changes the
// exponent.

// test_fx.cpp and test_fx64.cpp include this header, which is what compiles it:
// moba_fx is an INTERFACE library and FILE_SET HEADERS is IDE metadata, not a
// compile. Keep those includes -- drop them and a break here ships silently,
// and doctest goes back to printing `CHECK( {?} == {?} )`. The same trap
// applies to every header in the project; strong_id.hpp went unchecked for
// exactly this reason until test_strong_id.cpp was written.

// TODO: [missing] vec2 and angle formatters, once those exist.

#include <format>
#include <moba/fx/fx.hpp>
#include <moba/fx/fx64.hpp>
#include <ostream>

namespace moba {
/// exact: i32 raw converts to double exactly, /2^16 only shifts the exponent
[[nodiscard]] constexpr double to_double(fx v) noexcept {
  return static_cast<double>(v.raw) / static_cast<double>(fx::SCALE);
}

/// approximate above |raw| 2^53: i64 raw exceeds a double's 53-bit mantissa
[[nodiscard]] constexpr double to_double_lossy(fx64 v) noexcept {
  return static_cast<double>(v.raw) / static_cast<double>(fx64::SCALE);
}
} // namespace moba

// specialisations must be at global scope, and format() must be const.
template <> struct std::formatter<moba::fx> : std::formatter<double> {
  auto format(moba::fx v, std::format_context &ctx) const {
    // the base writes the number and hands back the cursor; append after it
    auto out = std::formatter<double>::format(moba::to_double(v), ctx);
    return std::format_to(out, " fx({})", v.raw);
  }
};

template <> struct std::formatter<moba::fx64> : std::formatter<double> {
  auto format(moba::fx64 v, std::format_context &ctx) const {
    auto out = std::formatter<double>::format(moba::to_double_lossy(v), ctx);
    return std::format_to(out, " fx64({})", v.raw);
  }
};

namespace moba {
inline std::ostream &operator<<(std::ostream &os, fx v) {
  return os << std::format("{}", v);
}

inline std::ostream &operator<<(std::ostream &os, fx64 v) {
  return os << std::format("{}", v);
}

} // namespace moba
