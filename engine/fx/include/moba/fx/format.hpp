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

// Every test file in fx/ includes this header, which is what compiles it:
// moba_fx is an INTERFACE library and FILE_SET HEADERS is IDE metadata, not a
// compile. Keep those includes -- drop them and a break here ships silently,
// and doctest goes back to printing `CHECK( {?} == {?} )`. The same trap
// applies to every header in the project; strong_id.hpp went unchecked for
// exactly this reason until test_strong_id.cpp was written.

//
// TODO: [decide] shapes.hpp has no formatters. circle/segment/aabb/capsule are
//       all trivial compositions of vec2 and fx, so they are cheap to add;
//       the question is whether a hitbox is ever printed rather than asserted
//       on. Add when something needs to read one.

#include <format>
#include <moba/fx/fx.hpp>
#include <moba/fx/angle.hpp>
#include <moba/fx/fx64.hpp>
#include <moba/fx/vec2.hpp>
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

/// exact: raw is at most 65535, raw * 360 at most 23.6M -- both well inside a
/// double's mantissa -- and dividing by 65536 only shifts the exponent.
[[nodiscard]] constexpr double to_degrees(angle a) noexcept {
  return static_cast<double>(a.raw) * 360.0 / static_cast<double>(angle::SCALE);
}
}  // namespace moba

// specialisations must be at global scope, and format() must be const.
template <>
struct std::formatter<moba::fx> : std::formatter<double> {
  auto format(moba::fx v, std::format_context &ctx) const {
    // the base writes the number and hands back the cursor; append after it
    auto out = std::formatter<double>::format(moba::to_double(v), ctx);
    return std::format_to(out, " fx({})", v.raw);
  }
};

template <>
struct std::formatter<moba::fx64> : std::formatter<double> {
  auto format(moba::fx64 v, std::format_context &ctx) const {
    auto out = std::formatter<double>::format(moba::to_double_lossy(v), ctx);
    return std::format_to(out, " fx64({})", v.raw);
  }
};

// Degrees, because a bare 49152 does not read as three quarters of a turn.
// The raw follows in parentheses exactly as fx's does, since the raw is what a
// desync report needs to be comparable between machines -- the degrees are for
// the human and the raw is the evidence.
template <>
struct std::formatter<moba::angle> : std::formatter<double> {
  auto format(moba::angle a, std::format_context &ctx) const {
    auto out = std::formatter<double>::format(moba::to_degrees(a), ctx);
    return std::format_to(out, " deg angle({})", a.raw);
  }
};

// delegates each component to the fx formatter above rather than reprinting a
// number, so "how an fx looks" has one definition. Gives
// "(1.5 fx(98304), 2.25 fx(147456))", and the inherited parse() means the
// float spec still applies to both components.
//
// format() must not be static: the library calls it on a formatter object, and
// a static member would hide the base's overload set rather than extend it.
//
// advance_to is the part that is easy to get wrong. formatter<T>::format
// writes at ctx.out() and returns the new position; it does not move the
// context itself. Without advancing between components the second write starts
// where the first did.
template <>
struct std::formatter<moba::vec2> : std::formatter<moba::fx> {
  auto format(moba::vec2 v, std::format_context &ctx) const {
    auto out = std::format_to(ctx.out(), "(");
    ctx.advance_to(out);
    out = std::formatter<moba::fx>::format(v.x, ctx);
    out = std::format_to(out, ", ");
    ctx.advance_to(out);
    out = std::formatter<moba::fx>::format(v.y, ctx);
    return std::format_to(out, ")");
  }
};

namespace moba {
inline std::ostream &operator<<(std::ostream &os, fx v) {
  return os << std::format("{}", v);
}

inline std::ostream &operator<<(std::ostream &os, fx64 v) {
  return os << std::format("{}", v);
}

inline std::ostream &operator<<(std::ostream &os, vec2 v) {
  return os << std::format("{}", v);
}

inline std::ostream &operator<<(std::ostream &os, angle a) {
  return os << std::format("{}", a);
}
}  // namespace moba
