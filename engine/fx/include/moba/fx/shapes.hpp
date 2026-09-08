#pragma once

// moba/fx/shapes.hpp -- collision primitives. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Needs fx and vec2. Phase 2 hitboxes are built
//       from this.
//
// Types (POD aggregates, no constructors):
//   circle  { vec2 c; fx r; }
//   segment { vec2 a, b; }
//   aabb    { vec2 min, max; }
//   capsule { segment seg; fx r; }   most bodies and hitboxes are one of these
//
// Predicates:
//   overlaps(circle, circle) / (aabb, aabb) / (circle, aabb) / (capsule,
//   capsule) contains(aabb, vec2) dist_sq_point_segment(vec2, segment) -> fx64
//   dist_sq_segment_segment(segment, segment) -> fx64
//   raycast(segment ray, circle) -> optional hit
//
// Notes:
//   - All of these are expressible without a square root. Keep it that way.
//   - Squared quantities are fx64 (see vec2.hpp). That propagates: a radius
//     check is `dist_sq(...) <= mul_wide(r, r)`, NOT `<= r * r`, which
//     overflows above r ~181. Write the helper once.
//
// TODO: [decide] Is exactly-touching a hit? `<` vs `<=`, one character, and it
//       must be identical across every predicate -- a hit that lands in one
//       and misses in another is a bug players will find. Fighting-game
//       convention is `<=`. Pin it with an exactly-touching test per predicate
//       and state the choice at the top of this file.
//
// TODO: [decide] Degenerate inputs: zero-length segment, zero-radius circle,
//       aabb with min > max. Each predicate needs a defined, identical answer
//       on both machines. Assert or defined result -- do not leave it to
//       whatever the arithmetic happens to do.
