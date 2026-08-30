#pragma once

// moba/fx/vec2.hpp -- 2D fixed-point vector. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Needs fx (works) and angle (not written).
//
// struct vec2 { fx x, y; }  -- POD aggregate, no invariants.
//
// Surface:
//   + - unary- += -=
//   operator*(fx) operator/(fx)
//   dot(a,b) -> fx
//   cross(a,b) -> fx            scalar z; sign gives which side of a line
//   length_sq(v) -> fx64        see the decide below
//   length(v) -> fx             needs isqrt
//   distance_sq(a,b) -> fx64
//   normalise(v) -> vec2
//   rotate(v, angle), perp(v), from_angle(angle, fx len), to_angle(v)
//
// TODO: [decide] length_sq must return fx64, not fx. fx's usable integer range
//       is about +/-32767, so two units 300 apart give 90000 and overflow.
//       Returning fx makes every range check wrong beyond ~181 units. This is
//       correctness, not optimisation, and it fixes the signature of every
//       range comparison in combat code.
//
// Notes:
//   - Prefer squared comparisons: `distance_sq(a,b) < r*r` avoids isqrt, and
//     hitbox tests run per-pair per-tick.
//   - normalise() of a zero vector has no correct answer. Pick one, document
//     it, {0,0} or assert. It must not be "undefined" -- both machines need
//     the same result.
//   - Accumulate in fx64 inside dot/length_sq and narrow once at the end.
//     Narrowing per-term loses precision in an operand-order-dependent way.
