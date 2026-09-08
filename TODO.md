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
- [ ] `fx/vec2.hpp` - whole file. Needs `fx` + `angle`. The `[decide]` in the
      header is settled in advance: `length_sq` returns `fx64`, not `fx`, or
      every range check past ~181 units is wrong.
- [ ] `fx/shapes.hpp` - whole file. Needs `fx` + `vec2`. **This is phase-2
      hitboxes.** Two `[decide]`s in the header: is exactly-touching a hit
      (`<` vs `<=`, and it must be the same answer in every predicate), and
      what the degenerate inputs do - zero-length segment, zero-radius circle,
      aabb with min > max.

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

- [ ] `core/types.hpp` - static_assert the alias widths and signedness. Free,
      turns a future target mismatch into a compile error. `test_types.cpp`
      checks `is_same_v` against the `<cstdint>` names, which is not the same
      claim as "`i32` is 4 bytes and signed on this target".

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
- [ ] `fx/fx.hpp` `[decide]` - `fixed_point` concept over `fx` and `fx64`.
      abs/min/max/clamp/sign now exist for both, so the second copy has arrived.
- [ ] `fx/fx.hpp` `[sequencing]` - dimensional units (metres vs seconds vs
      damage). Revisit end of phase 2, when the real unit set is known.
- [ ] `fx/fx.hpp` `operator*` - `mul_wide()` + `narrow()` open-coded; dedupe
      against `fx64.hpp`. Downgraded from a correctness risk to a tidiness one:
      the four-path agreement test now pins the duplicate against the original,
      so drift is caught rather than shipped.
- [ ] `fx/format.hpp` `[missing]` - `vec2` / `angle` formatters. Blocked on P2.

---

## Suggested order

1. ~~P0 `assert.hpp`~~ - done
2. ~~P1 rounding policy and the `_fx` literal~~ - done
3. ~~P3 test debt~~ - done bar the two items still listed there
4. P2 in order: `isqrt` -> `angle` -> `vec2` -> `shapes`
5. Extend both golden hashes to cover `vec2` as soon as it exists - lock
   determinism before the sim grows, not after
6. Everything else as its trigger fires

P2 is the answer to "when do I get to write logic." It is four files, none of
them large, and `shapes.hpp` is already gameplay.

Two things to carry INTO P2, learned from the work above:

- a new header needs a test that includes it, or it is never compiled at all
- a test that asserts something "must fail" needs to assert WHY it failed.
  `WILL_FAIL` and a bare non-zero exit both pass on the wrong failure; the
  compile-fail tests match on the specific diagnostic for this reason
