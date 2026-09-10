# moba - closed work

Condensed from `TODO.md`. One entry per closed item: what it was, and the fact
worth not rediscovering. The narrative is gone; the measurements and the
decisions are not. Line numbers are a record of where a bug WAS, not a pointer
to live code.

---

## Policy decisions everything else rests on

Reversing any of these means touching every file that does arithmetic.

- **Floor everywhere.** `detail::fdiv` in `fx.hpp`; all seven division sites
  route through it. Verified: `a*0.5 == a/2` and `a/2^k == a>>k` hold with 0
  disagreements over 571k values. Only `from_ratio(-1,3)` changed, raw -21845
  to -21846. This was a *consistency* bug, not a determinism one -- `/`
  truncating and `>>` flooring are both standard and agree on every machine;
  the risk was two spellings of the same arithmetic disagreeing.
  One documented exception: `angle::sin` rounds to nearest, because an
  approximation of a transcendental is not two spellings of one operation.
- **Overflow: abort in debug, wrap in release.** Wrapping, not saturating --
  saturation costs a branch per op, is not associative, and hides bugs.
  Division by zero is the sole clamp.
- **`dot`, `cross`, `length_sq`, `distance_sq` all return `fx64`.** `fx` tops
  out near 32767, so a product of two coordinates fails at 181 units apart and
  `dot(v,v)` disagreed with `length_sq(v)` at 200. Sets the signature of every
  range comparison in combat code.
- **Touching is a hit** -- `<=` in every `shapes.hpp` predicate, one character,
  identical everywhere.
- **Degenerates assert in debug with a defined release answer.** Applied in
  `shapes.hpp`. Still owed by `vec2::normalise`, which is why that one is open.

## engine/core

- **`assert.hpp`** (2026-09-05) -- six bugs, all in the include path of
  everything: a global-namespace leak, `assertion_failed` declared but never
  defined, a helper that went unused under `NDEBUG` and tripped `-Werror`,
  a duplicate include, and a handler that never received the stringified
  condition. Now wired end to end.
- **`types.hpp`** -- duplicate `#pragma once`, stray `<limits>`. Width and
  signedness are now `static_assert`ed *in the header*, not in a test: a test
  fails only on a target it is RUN on, a static_assert on any target it is
  COMPILED for. Uses `numeric_limits<T>::digits` rather than `sizeof`, so it
  catches padding and a non-8-bit byte. Also pins two's complement, arithmetic
  `>>` on i32/i64 (the whole floor policy rests on it), and
  `sizeof(usize) == sizeof(void*)`.
- **`strong_id.hpp`** -- private storage, explicit constructors both ways, no
  arithmetic, default construction is a sentinel rather than zero.

## engine/fx

- **`fx.hpp` `_fx` literal** (2026-09-05) -- *the* real determinism bug. The
  literal went through `long double`: 80-bit on x86-64, 128-bit on AArch64, so
  identical source produced different raws per target, feeding every golden
  hash. `1_fx` also did not compile at all. Now `consteval
  operator""_fx(const char*)` parsing digits as integers, with out-of-range as
  a COMPILE error and four compile-fail tests pinning the diagnostics.
- **`fx.hpp` shift dedupe** -- the Q32.32 -> Q16.16 shift had four spellings,
  two written as `fx::SHIFT` and two as `fx64::SHIFT - fx::SHIFT`. Both are 16
  only because Q32.32 is exactly two Q16.16 scale factors, so they would have
  diverged silently the day `fx` became Q8.24. Now `detail::unscale_fx`, one
  spelling, with a `static_assert` in each file. Range checks deliberately
  stayed at the call sites so `MOBA_ASSERT`'s `source_location` names the
  operation that overflowed, not the helper.
- **`fx.hpp` `fixed_point` concept** -- constrains SHAPE, not BEHAVIOUR; two
  types can satisfy it and still round differently. Agreement stays measured.
  The half that needed a real test was the negative: `test_fx64.cpp` carries a
  near-miss type that must be REJECTED, and every clause was verified to reject
  independently.
- **`fx64.hpp` `narrow()`** -- measured, not broken. Four paths agree on
  negatives across 6.8M products; "fx64: four-path rounding agreement" holds
  them there. `operator/(fx)` deliberately does not exist: no caller, do not
  add one.
- **`isqrt.hpp`** -- fixed iteration count, or a termination condition
  depending only on the input. A convergence test that could differ between
  compilers is a desync.
- **`vec2.hpp`** (2026-09-08) -- three defects, all surfaced within minutes of
  a test file existing. `distance_sq` computed `a.a - b.b` instead of
  `(a-b).(a-b)`: it returned 8 for a distance of 2, went negative when b was
  further, and was not symmetric. `length()` cast a u32 root to i32 unchecked,
  so `length((30000,30000))` came back as raw -1514510296. Three of four
  hand-written asserts were wrong and one could never fire -- `(x1-x2) +
  (y1-y2)` is evaluated in i64 and wraps *before* the widening to i128. All
  removed rather than corrected: routing through `mul_wide` inherits the right
  checks. Verified against an i128 reference: 677970 pairs for dot and cross,
  473850 for `distance_sq`, zero disagreements. gcc's `-Wshadow` rejected
  constructor parameters clang's did not.
- **`shapes.hpp`** (2026-09-08) -- three bugs, again only once a test compiled
  the header. `dist_sq_point_segment` never clamped t, so it measured to the
  infinite LINE: a capsule ending at x=10 reported a hit at x=384. The parallel
  branch of `dist_sq_segment_segment` pinned s=0 and returned 401 where 101 was
  right. And `denom = |u|^2|v|^2` is a *fourth* power of length, past `fx64`'s
  ceiling at ~215 units.
  The last is gone rather than patched: **in 2D the minimum between two
  non-intersecting segments is always at an endpoint**, so the parametric solve
  is unnecessary -- that algorithm is the 3D one, and only 3D needs it. Verified
  over 1,536,184 random pairs against brute force, zero disagreements.
  NOT a speed win, and recorded as such: capsule/capsule went from 246 to 307
  instructions and 11.2 to 12.2 ns. The gain is correctness and a removed
  overflow.
- **`angle.hpp`** (2026-09-10) -- BAM16 (`u16`, 65536 units per turn), sine
  table generated by integer CORDIC at consteval time, 1024 entries.
  Measured over ALL 65536 representable angles -- the entire input domain of
  sin and cos, so proofs rather than samples. clang and gcc agree bit for bit:

  | | |
  | --- | --- |
  | sin, cos vs libm | 0.81 fx LSB |
  | `sin(-a) == -sin(a)` | EXACT, zero violations |
  | atan2 round trip | 1 BAM unit |
  | atan2 vs libm | 1 BAM unit, magnitudes 1 .. 2^30 |
  | golden hash | `0x8388EF8F` |

  Three decisions not to re-litigate:
  **The table is stored at Q2.30, not `fx`.** `fx` is an `i32` anyway, so 14
  extra fractional bits cost nothing in memory and remove one of the two
  roundings `sin()` would otherwise pay. Measured 1.21 LSB with two roundings,
  0.81 with one -- and exact antisymmetry appeared only once the second was
  gone. The first version failed antisymmetry on 60928 of 65536 angles.
  **Cardinals are pinned twice.** The four table entries at 0/90/180/270 are
  overwritten with exact values, and `atan2` special-cases the four axes:
  CORDIC rotates on every step, so it approaches an axis without ever landing
  on it. Gameplay leans on facing east being exactly `(speed, 0)`.
  **`operator<` is deleted.** There is no total order on a circle that respects
  addition; callers go through `shortest_delta`. Exactly-opposite angles land
  on -32768 and so always turn the same way -- deterministic, and written down
  because two call sites disagreeing would be a desync.

## Closed 2026-09-10, second pass

- **`shapes.hpp` commuted overloads and `contains(circle, vec2)`** -- all three
  DELEGATE rather than reimplement. Two orders computing the same thing
  separately could disagree, and a hit that lands in one and misses in the
  other is exactly what the touching-is-a-hit rule exists to prevent.
  `contains` goes through `overlaps(c, circle{p, 0_fx})`, since the header
  already promises a zero-radius circle is a usable point probe.
- **`vec2::perp` is anticlockwise** -- `(x, y) -> (-y, x)`. Decided by an
  identity rather than by taste: `cross(a, b)` is already positive when b lies
  anticlockwise of a, so the ACW perpendicular gives
  `cross(v, perp(v)) == length_sq(v)`. The clockwise version gives
  `-length_sq(v)` and would leave two opposite notions of positive rotation in
  one library. Pinned by that identity over 20k vectors, not by an example.
- **`normalise(ZERO)` returns `vec2::ZERO`** -- asserts in debug, ZERO in
  release, matching `shapes.hpp`'s degenerate policy. ZERO rather than an
  invented `(1, 0)`, because the question is only which wrong answer is easiest
  to SEE: a zero is a character that does not move; a silent `(1, 0)` is one
  drifting east for a reason nobody can find. It previously returned
  `(I32_MAX, I32_MAX)`, which is merely what `fx` division by zero does.
- **`operator*(fx, vec2)`** and the **`angle` formatter**. The formatter prints
  degrees with the raw alongside: a desync report needs the raw, but nobody
  reads 49152 as three quarters of a turn.
- **Death tests for the fx degenerates** (`death_fx.cpp`). `atan2(0, 0)` and
  `normalise(0)` both abort, which a `CHECK` cannot express. The control is a
  real regression rather than a formality: the `atan2` assert was briefly
  written `x != 0 && y != 0`, which rejects every axis-aligned aim, and
  pointing straight east is both legal and common. De Morgan -- only the ORIGIN
  is degenerate, so the condition is `||`. Verified non-vacuous by
  reintroducing the `&&` and confirming the control fails.
- **`vec2` and `shapes` golden hashes** -- `0x32AE149C` and `0x745104BE`,
  agreeing across clang and gcc at -O0 and -O2. Five golden hashes now.

## Test infrastructure

- **Five golden hashes** -- `fx`, `fx64`, `angle`, `vec2`, `shapes`.
  Each folds ~100k mixed operations into one committed constant that every CI
  row checks, so cross-platform agreement is enforced with no artefact
  comparison anywhere. They prove *identical and unchanged*, never *correct* --
  a hash of consistently wrong values is a stable hash. That is why the
  reference sweeps exist alongside them.
- **Death tests** -- `MobaDeathTest.cmake` plus `moba_add_death_test()`. The
  trap: CTest fails a test on SIGABRT regardless of `PASS_REGULAR_EXPRESSION`,
  so the child cannot be the ctest COMMAND. The run is wrapped in `cmake -P`
  and the signal absorbed inside `execute_process`. Two controls with a TRUE
  condition assert the `NDEBUG` arm stays silent.
- **Compile-fail tests** -- each matches a substring of the specific `throw`
  message it is meant to trip. Without that they would pass on any compile
  error at all. Changing a message means changing the test; that coupling is
  what stops them going vacuous.
- **Four suite rules**, each learned the hard way, and all four now in
  `README.md`: a header with no test is never compiled; a test that must fail
  has to say why; no commas in `TEST_CASE` names; `TEST_CASE` names must be
  unique across the whole binary.
