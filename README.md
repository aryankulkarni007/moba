# moba

Codename. A deterministic 5v5 arena simulation in C++20.

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

`ctest` registers one entry per doctest `TEST_CASE`, so `ctest -R <pattern>`
selects individual cases.

## Layout

```
cmake/           build modules: warnings, compiler guard, test helper
engine/          game-agnostic libraries
  core/          types, assert, containers        -> #include <moba/core/...>
  fx/            fixed point, angles, vectors     -> #include <moba/fx/...>
tests/           shared doctest main()
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
| `core/result`                                  | spec only, deliberately. First caller is phase 2                                          |
| `fx/isqrt`, `fx/angle`, `fx/vec2`, `fx/shapes` | spec only. This is the critical path                                                      |

Open work is tracked two ways: `TODO.md` for anything spanning more than one
file, and `TODO:` comments in the headers for everything else --
`grep -rn "TODO:" engine`. Each comment carries a tag saying what kind of
decision it is: `[missing]`, `[decide]`, `[sequencing]`, `[duplication]`.

The determinism claim rests on the golden hashes in `test_fx.cpp` and
`test_fx64.cpp`. Each folds ~100k mixed operations into one committed
constant, and every CI row -- x86-64 and AArch64, Clang and GCC, every preset
-- checks that same constant. Two rows disagreeing is a red build.
