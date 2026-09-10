// Death tests for the degenerate arms of <moba/fx/angle.hpp> and
// <moba/fx/vec2.hpp>.
//
// NOT a doctest file, and deliberately not linked against the shared test
// main. Every interesting case here ends in std::abort(), which takes the
// whole process with it, so these cannot share a binary with anything else.
//
// Driven from CMake by moba_add_death_test(). See the header of
// engine/core/tests/death_assert.cpp for why the indirection through
// `cmake -P` exists -- CTest may fail a test on SIGABRT regardless of
// PASS_REGULAR_EXPRESSION, so the child cannot be the ctest COMMAND.
//
// The case is chosen by argv[1] rather than by building one executable per
// case, because each executable can only die once.

#include <cstdio>
#include <cstring>
#include <moba/fx/angle.hpp>
#include <moba/fx/vec2.hpp>

namespace {

using namespace moba;

// atan2(0, 0) has no answer: every direction is equally wrong. The header
// asserts, and returns ZERO in release. A CHECK cannot express this -- the
// process is gone before doctest can report anything.
void atan2_of_the_origin() {
  const angle a = atan2(fx{}, fx{});
  std::printf("atan2 of the origin returned %u\n", a.raw);
}

// normalise(0) is the other degenerate with no correct answer. Same policy:
// assert in debug, vec2::ZERO in release.
void normalise_of_the_zero_vector() {
  const vec2 v = normalise(vec2::ZERO);
  std::printf("normalise of zero returned %d %d\n", v.x.raw, v.y.raw);
}

// THE CONTROLS. Without them, a version that aborted on ANY input would pass
// both cases above.
//
// The first is a regression rather than a formality. The assert in atan2 was
// briefly written `x.raw != 0 && y.raw != 0`, which rejects every axis-aligned
// aim -- and pointing straight east is not only legal, it is one of the most
// common calls the game will make. De Morgan: only the ORIGIN is degenerate,
// so the condition is `||`. This case is what pins that.
void atan2_on_an_axis_is_legal() {
  const angle a = atan2(fx{}, fx::from_int(1));
  std::printf("atan2 on the +x axis returned %u\n", a.raw);
}

void normalise_of_a_real_vector_is_legal() {
  const vec2 v = normalise(vec2{ fx::from_int(3), fx::from_int(4) });
  std::printf("normalise of (3,4) returned %d %d\n", v.x.raw, v.y.raw);
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: death_fx <case>\n");
    return 2;
  }

  const char *which = argv[1];

  if (std::strcmp(which, "atan2_origin") == 0) {
    atan2_of_the_origin();
  } else if (std::strcmp(which, "normalise_zero") == 0) {
    normalise_of_the_zero_vector();
  } else if (std::strcmp(which, "atan2_axis") == 0) {
    atan2_on_an_axis_is_legal();
  } else if (std::strcmp(which, "normalise_real") == 0) {
    normalise_of_a_real_vector_is_legal();
  } else {
    std::fprintf(stderr, "death_fx: unknown case '%s'\n", which);
    return 2;
  }

  // Reached in two situations, and the driver tells them apart by build type:
  //   - a control case, in either arm
  //   - an abort case under NDEBUG, where the macro compiles to nothing
  return 0;
}
