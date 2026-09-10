# moba - pending work

Last revised 2026-09-10. Closed work lives in `DONE.md` -- condensed, with the
measurements and decisions kept and the narrative dropped.

Foundation: `types`, `assert`, `strong_id`, `fx`, `fx64`, `isqrt`, `vec2`,
`shapes`, `angle`, `format` are written AND tested -- **138 ctest entries green
under all four presets**. One spec-only stub remains, `core/result.hpp`.

Ordering is the path to PoC-0 (`DESIGN.md` 10), then work whose trigger has
already fired, then work whose has not. Not by severity: nothing is broken.

Open items reference the `TODO: [tag]` in the header rather than a line number
-- `grep -rn "TODO: \[" engine` -- because line numbers in this file drifted
within two commits last time.

---

## 0. Settle the world scale

**Everything downstream inherits this, and it is currently unratified.**

`DESIGN.md` proposes but does not fix: character radius **16 units**, map
**4096 x 4096** -- 256 radii per side, about 11% larger than League. Corner to
corner that is a `distance_sq` of 3.4e7 against `fx64`'s 2.1e9 ceiling, so 64x
headroom, and it gives a clean 256 x 256 pathfinding grid at 16 units per tile.

What it does NOT touch: `fx` arithmetic and the three golden hashes are
scale-free -- they operate on raws and do not care what a unit means.

What it does touch: every gameplay constant, the grid size, and how much room
`shapes.hpp` has before its intermediate products bite.

- [ ] Ratify the scale.
- [ ] `fx/fx.hpp` `[sequencing]` - dimensional units (metres vs seconds vs
      damage). Its stated trigger, "end of phase 2", has fired. The real
      blocker is the scale above; the unit set follows from it.

## 1. The path to the first pixel

`DESIGN.md` 10.2 phase 1, in dependency order. At the end of this list PoC-0 is
buildable and the go/no-go on the whole design is answerable.

- [ ] `fx/shapes.hpp` - **height bands**. A body occupies a set, an attack is
      authored to one, and an overlap becomes
      `overlaps_2d(a,b) && bands_intersect(a,b)`. Bitmask, not an interval:
      three bands fit in three bits, multi-band occupancy is free
      (`LOW|MID|HIGH` standing, `MID|HIGH` airborne) and the test is one AND.
      The existing 2D predicates are untouched -- this extends, it does not
      rewrite. See `DESIGN.md` 4.2 and 4.3.
- [ ] `fx/vec2.hpp` `[missing]` - `rotate`, `from_angle`, `to_angle`.
      `angle.hpp`'s first real callers, and `from_angle` is the exact unit
      vector `normalise` cannot produce for short inputs.
- [ ] `core/slot_map` - whole file. Generational handles; presence and absence
      must be representable, because `DESIGN.md` 8 requires the entity set to
      be a parameter rather than an assumption. Settles the `strong_id`
      `[decide]` below as a side effect.
- [ ] `game/` - entity state and a pure `step()` on a fixed timestep. POD, no
      pointers, slot_map indices rather than addresses.
- [ ] Window, input and a debug renderer. Renderer is decided (Vulkan + SDL,
      `DESIGN.md` 10.3) but **PoC-0 needs none of it** -- plain `SDL_Renderer`
      drawing coloured shapes is about twenty lines and gets thrown away when
      real art exists. Write the Vulkan renderer when there is something worth
      rendering.
- [ ] **Frame-data debug overlay.** Not optional and not deferrable: current
      state, frame counter, active hitboxes drawn, advantage on hit. Frame data
      that cannot be seen cannot be tuned.

Two properties must hold from the first line of `game/`, because both are
rewrites rather than additions if deferred:

- **state is snapshot-able** -- POD, no pointers, indices not addresses
- **the entity set is a parameter** -- nothing iterates "the ten champions"
- **new arithmetic joins a golden hash before the sim depends on it.** There
  are five now -- `fx`, `fx64`, `angle`, `vec2`, `shapes`. The point of each is
  that a change nobody intended moves a committed constant; arithmetic outside
  one is arithmetic no CI row is comparing across platforms.

## 2. Deferred, each with its trigger

Working on these before the trigger fires is how two days become two weeks.

| Item                                                                        | Trigger                                                                                                                                     |
| --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `core/result.hpp` -- whole file                                             | first real caller, phase 2                                                                                                                  |
| `core/strong_id.hpp` `[decide]` generational layout (`{u32,u32}` vs packed) | writing `slot_map` -- **about to fire**, see section 1                                                                                      |
| `core/strong_id.hpp` `[missing]` printing                                   | the first id type with a real consumer                                                                                                      |
| `fx/shapes.hpp` `[decide]` `raycast(segment, circle)`                       | something needing a first-hit _point_ rather than a yes/no. Swept collision is `overlaps(capsule, capsule)`, which exists and takes no root |
| `fx/shapes.hpp` `[sequencing]` `sq(fx)` placement                           | a second caller outside `shapes.hpp`. A location question, not a behaviour one                                                              |
| `fx/format.hpp` `[decide]` shapes formatters                                | the first time a failing hitbox test is unreadable without one                                                                              |
| `fx/fx64.hpp` `[sequencing]` `operator/(fx)`                                | **answered: do not add it.** No caller exists. Listed so it stops being re-asked                                                            |

---

## Keeping this file honest

The 2026-09-10 audit found five wrong claims. Every one was a _"blocked on X"_
or _"not done yet"_ that survived X clearing -- nothing ever claimed done when
it was not. The decay is entirely one-directional, and it is predictable:
notes get written while blocked and nobody revisits them when the block lifts.

Since the notes name their blockers, the fix is mechanical. **When something
lands, grep for its name across `engine/` and this file before moving on.**
`angle.hpp` landing invalidated three separate notes in three different files.
