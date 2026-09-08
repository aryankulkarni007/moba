// MUST NOT COMPILE -- integer part is 32768; fx::MAX_INT is 32767.
//
// Driven by moba_add_compile_fail_test(), which requires the diagnostic to name
// the guard below in EXPECT -- so this file failing to build for some OTHER
// reason is a test failure too, not a pass. See <moba/fx/fx.hpp>, THE _fx
// LITERAL, and the EXPECT strings in this directory's CMakeLists.txt.
//
// Expected guard: "fx literal: integer part overflow"
#include <moba/fx/fx.hpp>

using namespace moba;

int main() {
  constexpr fx v = 32768_fx;
  return v.raw;
}
