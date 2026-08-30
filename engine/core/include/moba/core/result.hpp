#pragma once

// moba/core/result.hpp -- fallible return values. SPEC ONLY, not implemented.
//
// Scope: the tooling and I/O boundary ONLY. Never inside step(), never in
// World. The sim has no recoverable errors -- a bad state there is a bug, and
// the answer is MOBA_ASSERT and abort. A Result in sim code would imply a
// caller who can do something about it, mid-tick, and there isn't one.
//
// Named consumers, all outside the sim:
//   phase 2  TOML ability data parsing
//   phase 5  replay file loading, hash log parsing
//   phase 6  UDP socket setup, packet decode
//   any      CLI argument and config handling
//
// TODO: [sequencing] Whole file. First real caller is phase 2. Writing it now
//       is infrastructure ahead of demand -- the spec below is enough to code
//       against when the parser lands.
//
// Design:
//
//   Mirror std::expected<T, E> exactly. It is C++23 and this project is C++20,
//   so do not bump the standard for one type -- but matching the interface
//   means the eventual migration is `using Result = std::expected` and a
//   delete. Same names: has_value, value, error, value_or, and_then,
//   transform, or_else, operator bool, operator*, operator->.
//
//   [[nodiscard]] on the type, not per-function. A discarded Result is the
//   exact bug it exists to prevent.
//
//   Error type is an `enum class`, not a string. Strings allocate, cannot be
//   compared in a test, and encourage error messages as control flow. Pair the
//   enum with optional context at the reporting boundary if needed.
//
//   No exceptions in the API. Note the project cannot use -fno-exceptions
//   wholesale -- doctest's REQUIRE throws -- so this is a convention, not a
//   flag. If the sim target ever gets -fno-exceptions, that is per-target.
//
//   Provide an explicit unwrap() that asserts. Do not make operator* assert;
//   callers should be able to opt into the checked or unchecked form.
//
// TODO: [decide] Is Result<void, E> needed? Parsing that only validates, and
//       socket setup, both want it. std::expected supports it via a partial
//       specialisation and it roughly doubles the implementation. Decide
//       before starting rather than bolting it on.
//
// TODO: [decide] Trivially-destructible fast path. std::expected is trivially
//       destructible when T and E both are, which matters if a Result ever
//       ends up in a hot loop. Parsing is not hot. Skip it unless a profile
//       says otherwise, and write down that you skipped it deliberately.
