# moba - pending work

Generated 2026-08-30 from a repo-wide TODO sweep. Last revised 2026-09-08.
Ordering is by dependency and decay cost, not by file.

Status of the foundation: `fx.hpp`, `fx64.hpp`, `types.hpp`, `assert.hpp`,
`strong_id.hpp` and `format.hpp` are written AND tested -- 63 ctest entries
green under all four presets. Everything else in `fx/` and `core/` is a
spec-only stub: one line of code, the rest comment.

Line numbers appear only in the CLOSED P0/P1 items, where they are a record of
where a bug was, not a pointer to live code. Open items reference the `TODO:`
tag in the header instead -- `grep -rn "TODO: \[" engine` -- because line
numbers in this file drifted within two commits last time.

---

## P0 - broken now, and in the include path of everything

`assert.hpp` is included nearly everywhere and has four open `[BUG]` notes. It is
infrastructure that other work compiles against, so these come first.

- [x] `core/assert.hpp:50` - global namespace in a widely-included header. Same
      class of leak as the one in `strong_id.hpp`. Move into `moba::`.
- [x] `core/assert.hpp:44` - `assertion_failed` declared, never defined, never
      called. Link error the moment a macro actually reaches it.
- [x] `core/assert.hpp:16` - static function used only inside an assert becomes
      unused in `NDEBUG` builds → warning → error under the project's warning flags.
- [x] `core/assert.hpp:57` - `<source_location>` / `<string_view>` included twice.
- [x] `core/assert.hpp:53` - handler needs `cond` as well as `msg`; the macros
      stringify `#cond` but nothing receives it.
- [x] `core/assert.hpp:30` - finish wiring `assertion_failed`. Ties the above together.
- [x] `core/types.hpp:3` - duplicate `#pragma once`, stray `<limits>` placement.
      Two minutes.

## P1 - rounding and literal policy

Settled 2026-09-05. Only one of these was ever a determinism bug; the section
title used to claim all four were.

- [x] **Rounding direction - DECIDED: floor everywhere.** DONE. `detail::fdiv`
      exists in `fx.hpp` and all seven division sites route through it -
      `fx.hpp` `from_ratio`, `operator/(fx)`, `operator/(i32)`, `div_sat`;
      `fx64.hpp` `operator/(fx64)`, `operator/(i32)`, `div_sat`. Comments and
      code now agree.
      Verified on a patched copy: `a*0.5 == a/2` and `a/2^k == a>>k` both hold
      with 0 disagreements over 571k values; all four multiply paths still
      agree; `from_ratio(-1,3)` moves from raw -21845 to -21846, the only
      existing value that changes.
      NOTE: this was a _consistency_ bug, not a determinism one. `/` truncating
      and `>>` flooring are both fully specified by the standard and agree on
      every machine. The risk was two spellings of the same arithmetic
      disagreeing, and a golden hash baking that in.

- [x] `fx/fx.hpp:366` - **the one real determinism item.** The `_fx` literal
      goes through `long double`: 80-bit on x86-64, 128-bit quad on AArch64, so
      the same source literal yields different raw values per target and those
      raws feed every golden hash. Two further defects in the same three lines:
      `static_cast` truncates where everything now floors, and `1_fx` does not
      compile at all (a `long double` UDL only matches floating literals).
      Fix: `consteval operator""_fx(const char*)` parsing digits as integers.
      Keep the property that an out-of-range literal is a COMPILE error.
      Until then `from_ratio(num, den)` is the exact, portable spelling.

- [x] `fx/fx64.hpp` `narrow()` - measured, not broken. `fx::operator*`,
      `narrow(mul_wide)`, `narrow(fx64*fx64)` and `narrow(fx64*fx)` agree on
      negatives across 6.8M products. The test that holds them there now
      exists: "fx64: four-path rounding agreement" in `test_fx64.cpp`.

- [x] `fx/fx64.hpp` `operator/(fx)` - no action. The existing note already
      resolves it: no caller exists, do not add it. Retagged `[decide]` →
      `[sequencing]` so it stops reading as an open question.

## P2 - the critical path to writing actual game logic

Four small files. Strict dependency order - each needs the one above it. At the
end of this list you are writing phase-2 hitbox code, which is logic, not types.

- [x] `fx/isqrt.hpp` - whole file. **No dependencies, start here.** Blocks
      `vec2::length` and `vec2::normalise`. The loop must run a fixed number of
      iterations, or terminate on a condition depending only on the input - a
      convergence test that could differ between compilers is a desync.
- [ ] `fx/angle.hpp` - whole file. Needs `fx` (done). Blocks `vec2::rotate`,
      `from_angle`, `to_angle`, and all aim handling. Two open calls in the
      header: `[decide]` what `operator<` means on an angle (there is no total
      order on a circle), and `[phase 1]` quantise aim input to this type at
      capture, day one.
- [x] `fx/vec2.hpp` - core done; rotate/from_angle/to_angle still need angle,
      and `perp` plus `operator*(fx, vec2)` are unwritten. `length_sq` returns
      `fx64` as decided, and so do `dot` and `cross`, for the same reason:
      `dot(v, v)` is `length_sq(v)` and the two disagreed at 200 units while
      dot returned fx.
      Three defects found and fixed once `test_vec2.cpp` existed to compile the
      header at all. `distance_sq` computed `a.a - b.b` rather than
      `(a-b).(a-b)`: it returned 8 for a distance of 2, went negative when the
      second point was further, and was not symmetric. `length()` cast a u32
      root to i32 with no check, and `length((30000, 30000))` came back as raw
      -1514510296; isqrt reaches ~3.03e9 here because length_sq is a sum of two
      full-width products and reaches 2^63, where sqrt(fx) tops out at 2^47.
      Three of the four hand-written asserts were wrong, one of them
      unfireable: `(x1 - x2) + (y1 - y2)` is evaluated in i64 and wraps before
      the widening conversion to i128, so `fits_i64` always saw a value in
      range. They are gone rather than corrected -- routing through `mul_wide`
      and `fx64::operator+` inherits the right checks, and the only assert
      written by hand is now the narrowing one in `length()`.
      Verified against an i128 reference computed from the raws: 677970 pairs
      for dot and cross, 473850 for distance_sq, zero disagreements.
      gcc's -Wshadow rejected the constructor parameters where clang's did not;
      that surfaced only because this was the first time anything compiled the
      header.
- [x] `fx/shapes.hpp` - done bar raycast and three commuted spellings.
      Both `[decide]`s answered at the top of the file: touching is a hit
      (`<=` everywhere, checked once per predicate), and degenerates assert in
      debug with a defined release answer.
      Three bugs, all found only once `test_shapes.cpp` existed to compile the
      header. `dist_sq_point_segment` never clamped t, so it measured to the
      infinite LINE: a capsule ending at x=10 reported a hit at x=384. The
      parallel branch of `dist_sq_segment_segment` pinned s=0 and returned 401
      where 101 was right. And `denom = |u|^2|v|^2` is a fourth power of
      length, which passes fx64's 2.1e9 ceiling at ~215 units and aborted in
      debug at 300.
      The last one is gone rather than patched: in 2D the minimum between two
      non-intersecting segments is always at an endpoint, so the parametric
      solve is unnecessary. Verified over 1,536,184 non-intersecting random
      pairs, zero disagreements with brute force. That algorithm is the 3D one;
      only 3D needs it, because skew segments can be closest at interior
      points of both.
      Point-segment is now three regions chosen by sign, with t never formed:
      two of the three need no division, and the middle one is
      cross(ap,ab)^2/|ab|^2 directly rather than the Lagrange-equivalent
      subtraction, which cancels catastrophically near the line.
      NOT a speed win, and worth recording as such: capsule/capsule went from
      246 to 307 instructions and 11.2 to 12.2 ns per call on a mixed
      workload. Same four i128 divisions worst case, now skippable at runtime.
      The gain is correctness and the removed overflow, not throughput.

- [x] `fx/tests/test_shapes.cpp` - 16 cases. Two independent references: an
      algebraic one from the fx raws in i128, and a sampled minimum that walks
      the actual segment, which catches a formula that is self-consistently
      wrong. All three regressions are checked by value.

## P3 - test debt

Mostly cleared 2026-09-08. `test_fx.cpp` and `test_fx64.cpp` now mirror each
other case for case, `test_strong_id.cpp` exists, and the golden hashes are in
place. What is left is listed below; do not re-open the closed items.

- [x] `fx/tests/test_fx.cpp` - the real suite. Round trip, rounding, compound
      operators, division, algebra, ordering, constexpr, vs-double, the free
      functions, the saturating family and the `_fx` literal group.
- [x] `fx/tests/test_fx64.cpp` - mirrors the above, plus `mul_wide` exactness,
      `widen`/`narrow` round trip, `narrow` wrapping vs `narrow_sat` clamping,
      the mixed-fx operators, and the four-path rounding agreement promoted
      from P1.
- [x] The golden hash. Note this did NOT become `test_golden.cpp`: there is one
      hash per width, living beside the operators it exercises. The reason the
      separate file was wanted was cross-preset comparison, and that turned out
      not to need a file -- the constant is committed once and every preset
      checks the same one, so disagreement is already a red build. Extend the
      operand mix in place when `vec2` lands rather than adding a third file.
- [x] `core/tests/test_assert.cpp` - covers `MOBA_ASSERT` in both arms.
- [x] `core/tests/test_strong_id.cpp` - and it is what COMPILES `strong_id.hpp`
      at all. An INTERFACE library plus `FILE_SET HEADERS` never compiles a
      header, so an unincluded header is an unchecked one. Any new header needs
      a test that includes it for that reason alone.
      Demonstrated again by `vec2.hpp`, which carried a wrong `distance_sq`, a
      wrong narrowing in `length()`, three bad asserts and a -Wshadow error
      until `test_vec2.cpp` gave it a translation unit.

- [x] `fx/tests/test_vec2.cpp` - 15 cases. The algebra each function claims
      (dot symmetric, cross antisymmetric, the axis and parallel identities),
      sweeps against an i128 reference built from the raws, the floor
      postcondition for `length` via `mul_wide`, and both regressions by value
      so they cannot return. Sweep guards assert the exact iteration count
      rather than a lower bound, so a loop that stops early fails instead of
      reporting a clean run over fewer inputs than intended.
- [x] `core/assert.hpp` - the death test. DONE.
      `engine/core/tests/death_assert.cpp` plus `cmake/MobaDeathTest.cmake`
      and `moba_add_death_test()`. The trap worth remembering: CTest may fail
      a test on SIGABRT regardless of `PASS_REGULAR_EXPRESSION`, so the child
      cannot be the ctest COMMAND -- the run is wrapped in `cmake -P` and the
      signal is absorbed inside `execute_process`. Matches on the printed
      message, not on a bare non-zero exit, for the same reason the
      compile-fail tests do. Two controls with a TRUE condition assert the
      NDEBUG arm stays silent. Also pins `-fmacro-prefix-map`, which nothing
      tested before.

- [x] `core/types.hpp` - static_assert the alias widths and signedness. DONE,
      in the header rather than in `test_types.cpp`: a test only fails on a
      target the tests are RUN on, a static_assert fails on any target the
      project is COMPILED for. Uses `numeric_limits<T>::digits`, not `sizeof`,
      because digits counts VALUE bits and so catches padding and a non-8-bit
      byte. Also pins two's complement, arithmetic `>>` on i32/i64 (the whole
      floor policy rests on it), `sizeof(i128) == 16` via a sign probe since
      `numeric_limits` is not specialised for `__int128` outside GNU mode, and
      `sizeof(usize) == sizeof(void*)`.

## P4 - deferred on purpose. Do not touch yet.

Each of these has a real trigger. Working on them before the trigger fires is how
two days become two weeks.

- [ ] `core/result.hpp` - whole file. Your own note: first real caller is phase 2.
      Write it when phase 2 needs it.
- [ ] `core/strong_id.hpp` `[decide]` - generational handle layout (`{u32,u32}`
      vs packed). A slot_map question, not an id question. Decide when writing
      slot_map. The accessors are already methods rather than public fields, so
      the choice stays reversible.
- [ ] `core/strong_id.hpp` `[missing]` - printing. Same reason as fx: a bare
      number in a trace log does not say which id space it belongs to. Do it
      when the first id type gets a real consumer, in phase 1.
- [x] `fx/fx.hpp` `[decide]` - `fixed_point` concept over `fx` and `fx64`.
      DONE. Defined at the bottom of `fx.hpp`; `fx.hpp` asserts `fx`, `fx64.hpp`
      asserts `fx64`, because the include graph runs one way and fx.hpp does not
      know fx64 exists. So a member added to one and forgotten on the other
      fails in the file that forgot it.
      It constrains SHAPE, not BEHAVIOUR -- two types can satisfy it and still
      round differently, which is the desync. Agreement stays measured, by the
      four-path test and the golden hashes.
      The half that needed a real test is the negative: a concept with a typo
      still satisfies both static_asserts. `test_fx64.cpp` carries a near-miss
      type (right storage, half the surface) that must be REJECTED -- same
      lesson as the compile-fail tests. Verified each clause rejects
      independently: dropping `trunc_to_int`, `round_to_int`, any one `_sat`,
      `sign`, `abs`, `clamp`, `n * v`, `operator-=`, `EPSILON`, `SCALE`, or
      adding a second data member each fails it on its own.
      Deliberately outside the concept, and said so in the header:
      floor/ceil/round/frac/lerp (fx only, on purpose), `from_ratio` (consteval,
      no fx64 literal syntax), widen/narrow (a relation between widths, not a
      property of one), MAX_INT/MIN_INT (fx64::from_int is total, no guard).
- [ ] `fx/fx.hpp` `[sequencing]` - dimensional units (metres vs seconds vs
      damage). Revisit end of phase 2, when the real unit set is known.
- [x] `fx/fx.hpp` `operator*` - dedupe against `fx64.hpp`. DONE, but not the
      way the note assumed. `operator*` is a member of `fx`, so it cannot call
      `mul_wide`/`narrow` -- fx64 does not exist yet at that point in the
      include graph. What moved is the part that could actually drift: the
      Q32.32 -> Q16.16 shift is now `detail::narrow_q32` in `fx.hpp`, templated
      so the i128 path shares it too. Four spellings collapsed to one --
      `fx::operator*`, `fx64::operator*(fx)`, `narrow()` and `narrow_sat()`.
      Two of those wrote the shift as `fx::SHIFT` and two as
      `fx64::SHIFT - fx::SHIFT`; both are 16 only because Q32.32 is exactly two
      Q16.16 scale factors, and they would silently diverge the day fx moved to
      Q8.24. That was the latent bug, and it is now a static_assert in each
      file instead.
      The range check deliberately stayed at the call sites: `MOBA_ASSERT`
      reports `source_location`, so hoisting it would make every overflow abort
      name the helper rather than the operation that overflowed.
      The four-path agreement test and both golden hashes still pass unchanged,
      which is what says the dedupe was behaviour-preserving.
- [ ] `fx/format.hpp` `[missing]` - `vec2` / `angle` formatters. Blocked on P2.

Three P4 items above are closed. They were the ones whose trigger had already
fired -- a second copy of the surface existed, and a fourth spelling of the
shift existed -- which is what P4 was waiting for, not a date.

---

## Suggested order

1. ~~P0 `assert.hpp`~~ - done
2. ~~P1 rounding policy and the `_fx` literal~~ - done
3. ~~P3 test debt~~ - done bar the two items still listed there
4. P2 is done except `angle`, which is a side branch rather than a link in
   the chain and is the largest item left in `fx/` (constexpr trig LUT by
   CORDIC or a fixed-point polynomial, atan2, plus the open `operator<`).
   Defer it until phase 1 aim handling asks, so the requirements are known
   rather than guessed.
5. Extend both golden hashes to cover `vec2` as soon as it exists - lock
   determinism before the sim grows, not after
6. Everything else as its trigger fires

P2 is the answer to "when do I get to write logic." It is four files, none of
them large, and `shapes.hpp` is already gameplay.

Three things carried into P2, and the first one cost twice:

- a new header needs a test that includes it, or it is never compiled at all.
  Ignored for `vec2.hpp` and again for `shapes.hpp`. Between them that hid six
  wrong formulas, four bad asserts, one -Wshadow error and one overflow. Every
  single one surfaced within minutes of the test file existing.
- a test that asserts something "must fail" needs to assert WHY it failed.
  `WILL_FAIL` and a bare non-zero exit both pass on the wrong failure; the
  compile-fail tests match on the specific diagnostic for this reason
- an algorithm copied from a reference carries that reference's assumptions.
  The segment-segment solver assumed 3D and brought a fourth-power term with
  it; the Wikipedia isqrt assumed a different loop start. Write down what the
  source assumes before transcribing it.
