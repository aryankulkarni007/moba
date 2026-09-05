# moba — pending work

Generated 2026-08-30 from a repo-wide TODO sweep.
Ordering is by dependency and decay cost, not by file.

Status of the foundation: `fx.hpp` and `fx64.hpp` are written (195 / 191 lines of
code). `types.hpp` and `format.hpp` are written. Everything else in `fx/` and
`core/` is a spec-only stub — one line of code, the rest comment.

---

## P0 — broken now, and in the include path of everything

`assert.hpp` is included nearly everywhere and has four open `[BUG]` notes. It is
infrastructure that other work compiles against, so these come first.

- [x] `core/assert.hpp:50` — global namespace in a widely-included header. Same
      class of leak as the one in `strong_id.hpp`. Move into `moba::`.
- [x] `core/assert.hpp:44` — `assertion_failed` declared, never defined, never
      called. Link error the moment a macro actually reaches it.
- [x] `core/assert.hpp:16` — static function used only inside an assert becomes
      unused in `NDEBUG` builds → warning → error under the project's warning flags.
- [x] `core/assert.hpp:57` — `<source_location>` / `<string_view>` included twice.
- [x] `core/assert.hpp:53` — handler needs `cond` as well as `msg`; the macros
      stringify `#cond` but nothing receives it.
- [x] `core/assert.hpp:30` — finish wiring `assertion_failed`. Ties the above together.
- [x] `core/types.hpp:3` — duplicate `#pragma once`, stray `<limits>` placement.
      Two minutes.

## P1 — determinism decisions that get more expensive every day

These are cheap to settle now and brutal to retrofit. Each one is a policy every
future operation has to obey, so the cost scales with how much code exists when
you fix it. This is the category that actually justifies the time spent on types.

- [ ] `fx/fx.hpp:64` — **pick one rounding direction and make every operation obey
      it.** The single highest-decay item in the repo. Everything downstream
      inherits whatever is decided here.
- [ ] `fx/fx.hpp:332` — literal goes through `long double`: 80-bit on x86-64,
      64-bit elsewhere. Cross-architecture divergence baked into a constant.
- [ ] `fx/fx64.hpp:270` — `>> 16` floors, matching `fx::operator*`, but nothing
      verifies the two stay in agreement. Needs a test, not just a comment.
- [ ] `fx/fx64.hpp:180` — decide whether `operator/(fx)` on a Q32.32 total exists.

## P2 — the critical path to writing actual game logic

Four small files. Strict dependency order — each needs the one above it. At the
end of this list you are writing phase-2 hitbox code, which is logic, not types.

- [ ] `fx/isqrt.hpp` — whole file. **No dependencies, start here.** Blocks
      `vec2::length` and `vec2::normalise`.
- [ ] `fx/angle.hpp` — whole file. Needs `fx` (done). Blocks `vec2::rotate`,
      `from_angle`, `to_angle`, and all aim handling. - `:30` decide what `operator<` means on an angle (no total order on a circle) - `:36` quantise aim input to this type at capture, day one
- [ ] `fx/vec2.hpp` — whole file. Needs `fx` + `angle`. - `:20` `length_sq` returns `fx64`, not `fx` — `fx`'s integer range overflows
- [ ] `fx/shapes.hpp` — whole file. Needs `fx` + `vec2`. **This is phase-2 hitboxes.** - `:27` is exactly-touching a hit? `<` vs `<=` - `:33` degenerate inputs: zero-length segment, zero-radius circle

## P3 — test debt

`test_fx.cpp` is 6 lines of code. `test_types.cpp` proves nothing yet, by its own
admission. The golden hash is the only thing that will ever catch a determinism
regression automatically.

- [ ] `fx/tests/test_fx.cpp:12` — the real suite. Write the test, watch it fail,
      then fix the matching TODO in `fx.hpp`.
- [ ] `fx/tests/CMakeLists.txt:4` — `test_fx64.cpp`: `mul_wide` exactness,
      `narrow()` saturation.
- [ ] `fx/tests/CMakeLists.txt:6` — `test_golden.cpp`: the ~100k mixed-operation
      hash. **Highest-value test in the project.** Locks determinism across
      compilers and architectures; without it P1 decisions silently rot.
- [ ] `core/tests/test_types.cpp:18` — cover `MOBA_ASSERT` once `assert.hpp` is done.
- [ ] `core/types.hpp:44` — static_assert the alias widths and signedness. Free,
      turns a future target mismatch into a compile error.

## P4 — deferred on purpose. Do not touch yet.

Each of these has a real trigger. Working on them before the trigger fires is how
two days become two weeks.

- [ ] `core/result.hpp` — whole file. Your own note: first real caller is phase 2.
      Write it when phase 2 needs it.
- [ ] `core/strong_id.hpp:66` — generational handle layout (`{u32,u32}` vs packed).
      A slot_map question, not an id question. Decide when writing slot_map. Make
      the accessors methods rather than public fields and the choice stays reversible.
- [ ] `fx/fx.hpp:359` — `fixed_point` concept over `fx` and `fx64`.
- [ ] `fx/fx.hpp:364` — dimensional units (metres vs seconds vs damage).
- [ ] `fx/fx.hpp:157` — `mul_wide()` + `narrow()` open-coded; dedupe against `fx64.hpp`.
- [ ] `fx/format.hpp:47` — `vec2` / `angle` formatters. Blocked on P2.

---

## Suggested order

1. P0 `assert.hpp` — half a day, unblocks clean compiles everywhere
2. `fx/fx.hpp:64` rounding policy — decide it, write it down, move on
3. P2 in order: `isqrt` → `angle` → `vec2` → `shapes`
4. `test_golden.cpp` as soon as `vec2` exists — lock determinism before the sim grows
5. Everything else as its trigger fires

P2 is the answer to "when do I get to write logic." It is four files, none of
them large, and `shapes.hpp` is already gameplay.
