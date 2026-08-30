#pragma once

// moba/fx/angle.hpp -- binary angle measure. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Blocks vec2::rotate/from_angle/to_angle and aim
//       quantisation in phase 1.
//
// Representation: u16, 65536 raw units = one full turn.
//   - wraparound is free and exact (unsigned overflow == modulo 2pi)
//   - common angles are exact (quarter turn == 16384)
//   - indexes a trig LUT directly
//
// Surface:
//   struct angle { u16 raw; }
//   from_turns / from_degrees      consteval, exact
//   + - += -= unary-               wrap by construction, do NOT clamp
//   shortest_delta(a, b) -> i16    signed shortest path; what turn rates want
//   constants QUARTER HALF FULL
//   sin(angle) -> fx, cos(angle) -> fx, atan2(fx y, fx x) -> angle
//
// Notes:
//   - Trig is a constexpr LUT, 1024 entries, linear interpolation. No build
//     step, no generated file to go stale.
//   - Generate the table with integer maths, NOT std::sin. std::sin is not
//     constexpr, and where a compiler allows it the result is the host libm's
//     and differs per platform. CORDIC or a fixed-point polynomial.
//   - Quarter table + symmetry vs full table is a clarity call, not a memory
//     one (1024 fx == 4 KB either way). Take the trivial one.
//
// TODO: [decide] What does operator< mean on an angle? No total order on a
//       circle respects addition -- 350 degrees is both greater than and just
//       before 10. A defaulted <=> silently gives raw-integer ordering.
//       Recommend: define == only, delete the relational operators, force
//       callers through shortest_delta. Easier to add later than remove.
//
// TODO: [phase 1] Quantise aim input to this type at capture, day one.
//       Un-quantised aim is the classic rollback desync and is expensive to
//       retrofit once replays exist.
