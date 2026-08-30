#pragma once
#include <moba/core/types.hpp>

using moba::u32;

// WARNING: need to choose one implementation

template <typename T, typename Tag> struct Strong {
  T value{};
  constexpr auto operator<=>(const Strong&) const = default;
};

template <typename Tag, typename Rep = u32> struct strong_id;

// using EntityId = Strong<u32, struct EntityIdTag>;
// using Frame = Strong<u32, struct FrameTag>;
// using PlayerId = Strong<u32, struct PlayerIdTag>;

// moba/core/strong_id.hpp -- type-safe identifiers. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Callers in phases 1-3: slot_map handles, entity
//       refs in World, hitbox source refs (counter-hit), ability indices,
//       player slots. All would be u32 today and all mutually substitutable --
//       damage(attacker, victim) with the arguments swapped type-checks.
//
// Why this is cheap: an id needs NO arithmetic. id + 1 is meaningless, two ids
// do not add. Construct, compare, convert back explicitly, done. Nothing to
// close over. Make arithmetic impossible -- the absence is the feature.
//
// Two builds:
//
//   (1) Scoped enum.  enum class entity_id : u32 {};
//       Free: distinct type, no implicit conversion either way, no arithmetic,
//       == and <=>, trivially copyable, 4 bytes, unique object reps. One line.
//       Costs: no member functions; static_cast to get the value out
//       (std::to_underlying is C++23, write a one-liner); printing needs a
//       helper.
//
//   (2) Tag template.  template <typename Tag, typename Rep = u32> struct strong_id;
//                      using entity_id = strong_id<struct entity_tag>;
//       The tag is declared inline in the alias, never defined, costs nothing.
//       Buys member functions. Costs machinery, and you must verify it stays
//       trivially copyable for World's memcpy.
//
//   Start with (1). Move to (2) when you write the same free function for the
//   fifth id type.
//
// Requirements either way:
//   - explicit both directions; an implicit u32 -> id conversion reintroduces
//     the exact bug this prevents
//   - sentinel is all-ones, not 0: 0 is a plausible real index, so a
//     zero-initialised World would be full of valid-looking refs to entity 0
//   - static_assert trivially_copyable, standard_layout,
//     has_unique_object_representations -- these live in World
//   - ordering, so ids can be sorted into a deterministic iteration order
//   - NO std::hash specialisation. No hash maps in the sim by design; not
//     providing the hook enforces that.
//   - printing, same reason as fx
//
// TODO: [decide] A generational handle is a different type from an id.
//       slot_map handles are {index, generation}: freeing bumps the generation
//       so stale handles resolve to null.
//         - struct of two u32, simple, 8 bytes
//         - one u32 packed index:24 generation:8 -- 4 bytes, fits the enum
//           approach, but wraps after 256 recycles of a slot and a stale
//           handle silently becomes valid again pointing at a different entity
//       Work out the worst case: how many times can one slot recycle in a
//       match at 60Hz with Rooke throwing daggers? Over 256 means more bits.
//       Decide before slot_map is written -- the handle type appears in every
//       consumer.
