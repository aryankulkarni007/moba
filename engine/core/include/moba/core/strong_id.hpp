#pragma once
#include <compare>
#include <limits>
#include <moba/core/types.hpp>
#include <type_traits>

// moba/core/strong_id.hpp -- type-safe identifiers.
//
// The problem: slot_map handles, entity refs in World, hitbox source refs
// (counter-hit), ability indices and player slots would all be u32, and all
// mutually substitutable -- damage(attacker, victim) with the arguments
// swapped type-checks and is silently wrong.
//
// Why this is cheap: an id needs NO arithmetic. id + 1 is meaningless, two ids
// do not add. Construct, compare, convert back explicitly, done. Nothing to
// close over. Making arithmetic impossible is not a cost here, it is the
// feature.
//
// WHICH BUILD THIS IS. Two were considered:
//
//   (1) Scoped enum.  enum class entity_id : u32 {};
//       Free: distinct type, no implicit conversion either way, no arithmetic,
//       == and <=>, trivially copyable, 4 bytes, unique object reps. One line.
//       Costs: no member functions; static_cast to get the value out
//       (std::to_underlying is C++23); printing needs a helper.
//
//   (2) Tag template, which is what is below. The tag is declared inline in
//       the alias, never defined, and costs nothing. Buys member functions --
//       valid() against the sentinel is the one that earns it, because every
//       consumer needs that check and (1) would spell it as a free function
//       per id type.
//
// The earlier note here said "start with (1), move to (2) when you write the
// same free function for the fifth id type". (2) is what got written. The
// trade it makes is machinery for valid()/get() being methods, and the
// static_asserts below are the price: (1) gave the layout guarantees for free,
// (2) has to be held to them explicitly.

namespace moba {
template <typename Tag, typename Rep = u32>
struct [[nodiscard]] strong_id {
public:
  using rep = Rep;

  // Default-constructs to the sentinel, NOT to zero. 0 is a plausible real
  // index, so a zero-initialised World would otherwise be full of
  // valid-looking references to entity 0.
  constexpr strong_id() noexcept = default;

  // explicit in both directions. An implicit u32 -> id conversion, or id ->
  // u32, reintroduces the exact bug the type exists to prevent.
  constexpr explicit strong_id(Rep v) noexcept : v_{ v } {}

  [[nodiscard]] constexpr explicit operator Rep() const noexcept { return v_; }
  [[nodiscard]] constexpr Rep      get() const noexcept { return v_; }
  [[nodiscard]] constexpr bool     valid() const noexcept { return v_ != null_v; }

  // Ordering, not just equality, so ids can be sorted into a deterministic
  // iteration order. A defaulted <=> also implicitly declares ==.
  //
  // Deliberately NO std::hash specialisation. There are no hash maps in the
  // sim by design -- iteration order would vary -- and not providing the hook
  // is what enforces that.
  constexpr std::strong_ordering operator<=>(const strong_id &) const = default;

private:
  static constexpr Rep null_v = std::numeric_limits<Rep>::max();
  Rep                  v_{ null_v };
};

namespace detail {
// Never defined. Exists only to instantiate the template below, since a
// static_assert cannot see a property of an uninstantiated class template.
struct strong_id_layout_probe;
}  // namespace detail

// Same reason as the block in <moba/fx/fx.hpp>: ids live inside the game
// state, and the game state is copied every tick for rollback and
// fingerprinted byte for byte to detect desync. That only works if an id IS
// its four bytes and nothing more -- padding the compiler may leave holding
// junk would make two ids that compare equal differ byte for byte, and report
// a desync that did not happen.
using strong_id_probe = strong_id<detail::strong_id_layout_probe>;
static_assert(sizeof(strong_id_probe) == 4);
static_assert(alignof(strong_id_probe) == 4);
static_assert(std::is_trivially_copyable_v<strong_id_probe>);
static_assert(std::is_standard_layout_v<strong_id_probe>);
static_assert(std::has_unique_object_representations_v<strong_id_probe>);

}  // namespace moba

// Aliases live with their consumers, not here -- this header has no business
// knowing what an entity is. The shape:
//
//   using entity_id = moba::strong_id<struct entity_tag>;
//   using player_id = moba::strong_id<struct player_tag>;
//
// TODO: [missing] Printing, same reason as fx: a bare number in a trace log
//       does not say which id space it belongs to. Blocked on nothing; do it
//       when the first id type gets a real consumer, in phase 1.
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
//       consumer. This is a slot_map question, not an id question; keeping
//       valid()/get() as methods rather than public fields is what keeps the
//       choice reversible.
