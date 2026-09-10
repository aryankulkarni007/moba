# moba

Codename. A deterministic 5v5 arena simulation in C++20.

A MOBA with fighting-game mechanics: closer to League than Dota, closer to
Tekken than Street Fighter.

The design is in [`DESIGN.md`](DESIGN.md) -- what the game is, and why each
decision constrains the engine. Read it before making a technical choice that
depends on a gameplay fact. If it does not answer the question, the answer is
unknown rather than inferable.

The hard requirement driving every technical decision: **two machines must
produce bit-identical simulation results.** Without that, rollback netcode and
replays are impossible. Hence fixed-point maths instead of floats, a pure
`step()` with no clock reads or allocation, POD state, and cross-compiler hash
agreement as the acceptance test.

## Prerequisites

|       |                                                                                                                                                                                   |
| ----- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| CMake | >= 3.24                                                                                                                                                                           |
| Ninja | any                                                                                                                                                                               |
| Clang | upstream LLVM. **Not Apple Clang** -- it reports `AppleClang` and the presets reject it (`brew install llvm`, put it ahead of `/usr/bin` on `PATH`)                               |
| GCC   | only for `gcc-release`. `brew install gcc` currently gives `g++-16`, which is what the preset names -- if brew moves on, update `CMakePresets.json` rather than working around it |

The compiler check is deliberate: a golden hash produced by an unexpected
compiler is not comparable with any other, so configure fails rather than
silently testing the same compiler twice.

## Build and test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

| Preset        | Build type     | Sanitizers                    | Purpose                                  |
| ------------- | -------------- | ----------------------------- | ---------------------------------------- |
| `debug`       | Debug          | ASan + UBSan, hardened stdlib | day-to-day                               |
| `release`     | Release        | --                            | golden hash reference                    |
| `release-san` | RelWithDebInfo | ASan + UBSan, hardened stdlib | bugs that only appear under optimisation |
| `gcc-release` | Release        | --                            | cross-compiler determinism check         |

`ctest` registers one entry per doctest `TEST_CASE`, under the case name alone.
The `TEST_SUITE` becomes a ctest *label*, not part of the name, so the two
selectors do different jobs:

```sh
ctest --preset debug -R 'four-path'    # one case, by name
ctest --preset debug -L 'fx/shapes'    # a whole suite, by label
ctest --preset debug -LE death         # everything except the death tests
```

## Layout

```
cmake/           build modules: warnings, compiler guard, test and death-test helpers
engine/          game-agnostic libraries
  core/          types, assert, ids               -> #include <moba/core/...>
  fx/            fixed point, vectors, shapes     -> #include <moba/fx/...>
tests/           shared doctest main()
.github/         the CI matrix described at the foot of this file
```

Libraries are `INTERFACE` (header-only) while they are header-only, `STATIC`
otherwise. Never `SHARED` -- it blocks cross-TU inlining and risks a flag
mismatch on the hot path, and a flag mismatch is a desync.

`game/` (sim, timeline, combat, net, replay) joins `engine/` as those land.

## Status

Phase 0.

|                                                |                                                                                           |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------- |
| `core/types`, `core/assert`, `core/strong_id`  | written, tested                                                                           |
| `fx`, `fx64`                                   | written, tested -- including the four-path rounding agreement and a golden hash per width |
| `fx/isqrt`, `fx/vec2`, `fx/shapes`             | written, tested. `shapes.hpp` is the phase-2 hitbox surface                               |
| `fx/angle`                                     | written, tested. BAM16 with a consteval CORDIC sine table -- sin/cos within 0.81 fx LSB, atan2 within 1 BAM unit         |
| `core/result`                                  | spec only, deliberately. First caller is phase 2                                          |

Work is tracked three ways: `TODO.md` for anything spanning more than one file,
`DONE.md` for closed items -- condensed to the measurements and decisions, so
nothing gets re-litigated -- and `TODO:` comments in the headers for
everything else --
`grep -rn "TODO:" engine`. Each comment carries a tag saying what kind of
decision it is: `[missing]`, `[decide]`, `[sequencing]`, `[phase 1]`.

The determinism claim rests on the golden hashes in `test_fx.cpp`,
`test_fx64.cpp` and `test_angle.cpp`. Each folds ~100k mixed operations into
one committed constant, and every CI row -- x86-64 and AArch64, Clang and GCC,
every preset -- checks that same constant. Two rows disagreeing is a red build.

Four rules the suite enforces, each learned the hard way:

- **A header with no test is never compiled.** The libraries are `INTERFACE`,
  and `FILE_SET HEADERS` is IDE metadata, not a build step. Every header needs
  a test that includes it for that reason alone.
- **A test that must fail has to say why.** `WILL_FAIL` and a bare non-zero
  exit both pass on the wrong failure, so the compile-fail tests match the
  specific diagnostic and the death tests match the printed message.
- **No commas in `TEST_CASE` names.** doctest's discovery emits an empty
  `LABELS` for those, so the case runs under a bare `ctest` but `ctest -L`
  silently skips it.
- **`TEST_CASE` names must be unique across the whole test binary**, not just
  within a `TEST_SUITE`. ctest registers one entry per case under the case name
  ALONE, so two suites sharing a name collapse into a single ambiguous entry
  that runs both. Hence the `fx64:` and `angle:` prefixes -- the files sharing
  the `test_fx` binary each prefix their cases.
