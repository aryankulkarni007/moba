#include <doctest/doctest.h>
#include <functional>  // std::hash -- reaches libc++ transitively, libstdc++ not
#include <moba/core/strong_id.hpp>
#include <type_traits>

// main() comes from tests/doctest_main.cpp.

// covers <moba/core/strong_id.hpp>.
//
// This file exists partly to COMPILE the header at all. moba_core is an
// INTERFACE library and FILE_SET HEADERS is IDE metadata, not a compile, so
// until something included strong_id.hpp nothing checked it -- including the
// layout static_asserts inside it, which are the reason it is safe to put an
// id in World. Same argument as the include of format.hpp in test_fx.cpp.

namespace {

using entity_id = moba::strong_id<struct entity_tag>;
using player_id = moba::strong_id<struct player_tag>;
using wide_id   = moba::strong_id<struct wide_tag, moba::u64>;

// The "does not compile" checks below have to go through concepts rather than
// a bare `!requires(entity_id a) { a + a; }`. A requires-expression only turns
// an ill-formed requirement into `false` when the types are DEPENDENT; with
// concrete types the requirement is checked immediately and a hard compile
// error is what you get. Routing through a concept's template parameters is
// what makes the substitution happen.
template <typename A, typename B>
concept addable = requires(A a, B b) { a + b; };
template <typename A, typename B>
concept multipliable = requires(A a, B b) { a * b; };
template <typename A>
concept incrementable = requires(A a) { ++a; };
template <typename A, typename B>
concept equality_comparable = requires(A a, B b) { a == b; };
template <typename A, typename B>
concept less_comparable = requires(A a, B b) { a < b; };

}  // namespace

TEST_SUITE("core/strong_id") {
  TEST_CASE("distinct tags are distinct types") {
    // The whole point. damage(attacker, victim) with the arguments swapped
    // must not compile, and that rests on this.
    static_assert(!std::is_same_v<entity_id, player_id>);
    static_assert(!std::is_convertible_v<entity_id, player_id>);
    static_assert(!std::is_convertible_v<player_id, entity_id>);
    CHECK(true);
  }

  TEST_CASE("conversions are explicit in both directions") {
    // An implicit conversion either way reintroduces the bug the type
    // prevents, so both are checked rather than assumed.
    static_assert(!std::is_convertible_v<moba::u32, entity_id>);
    static_assert(std::is_constructible_v<entity_id, moba::u32>);
    static_assert(!std::is_convertible_v<entity_id, moba::u32>);

    constexpr entity_id id{ 7 };
    static_assert(id.get() == 7);
    static_assert(static_cast<moba::u32>(id) == 7);
  }

  TEST_CASE("no arithmetic") {
    // The absence IS the feature: id + 1 is meaningless and two ids do not
    // add. Nothing here should ever start compiling.
    static_assert(!addable<entity_id, entity_id>);
    static_assert(!addable<entity_id, int>);
    static_assert(!incrementable<entity_id>);
    static_assert(!multipliable<entity_id, entity_id>);
    CHECK(true);
  }

  TEST_CASE("default construction is the sentinel rather than zero") {
    // 0 is a plausible real index, so a zero-initialised World must not be
    // full of valid-looking references to entity 0.
    constexpr entity_id def{};
    static_assert(!def.valid());
    static_assert(def.get() == ~moba::u32{ 0 });  // all ones

    static_assert(entity_id{ 0 }.valid());
    static_assert(entity_id{ 7 }.valid());

    // and the sentinel tracks Rep, not u32
    constexpr wide_id wide_def{};
    static_assert(!wide_def.valid());
    static_assert(wide_def.get() == ~moba::u64{ 0 });
  }

  TEST_CASE("comparison and ordering") {
    // Ordering exists so ids sort into a deterministic iteration order.
    static_assert(entity_id{ 1 } == entity_id{ 1 });
    static_assert(entity_id{ 1 } != entity_id{ 2 });
    static_assert(entity_id{ 1 } < entity_id{ 2 });
    static_assert(entity_id{ 2 } > entity_id{ 1 });
    static_assert(entity_id{ 1 } <= entity_id{ 1 });

    // an invalid id sorts above every real index, because the sentinel is
    // all-ones rather than 0
    static_assert(entity_id{ 0 } < entity_id{});
    CHECK(true);
  }

  TEST_CASE("comparison across tags does not compile") {
    static_assert(!equality_comparable<entity_id, player_id>);
    static_assert(!less_comparable<entity_id, player_id>);
    // ...while the same-tag comparisons that DO exist still work, so this is
    // not passing by accident.
    static_assert(equality_comparable<entity_id, entity_id>);
    static_assert(less_comparable<entity_id, entity_id>);
    CHECK(true);
  }

  TEST_CASE("byte representation") {
    // strong_id.hpp static_asserts these for one probe instantiation. Restated
    // here for the real id types, since it is per-instantiation and these are
    // the ones that end up inside the snapshotted World.
    static_assert(sizeof(entity_id) == 4);
    static_assert(alignof(entity_id) == 4);
    static_assert(std::is_trivially_copyable_v<entity_id>);
    static_assert(std::is_standard_layout_v<entity_id>);
    static_assert(std::has_unique_object_representations_v<entity_id>);

    static_assert(sizeof(wide_id) == 8);
    static_assert(std::is_trivially_copyable_v<wide_id>);
    static_assert(std::has_unique_object_representations_v<wide_id>);

    CHECK(sizeof(entity_id) == sizeof(moba::u32));
  }

  TEST_CASE("no std::hash specialisation") {
    // Not an oversight. There are no hash maps in the sim by design --
    // iteration order would vary between machines -- and withholding the hook
    // is what enforces it. If this ever starts failing, someone added a
    // specialisation and the reason it was absent has been lost.
    static_assert(!std::is_default_constructible_v<std::hash<entity_id>>);
    CHECK(true);
  }
}
