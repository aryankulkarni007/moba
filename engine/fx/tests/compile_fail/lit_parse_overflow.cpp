// MUST NOT COMPILE -- trips parse_u64's own u64 guard.
//
// Driven by moba_add_compile_fail_test(), which requires the diagnostic to name
// the guard below in EXPECT -- so this file failing to build for some OTHER
// reason is a test failure too, not a pass. See <moba/fx/fx.hpp>, THE _fx
// LITERAL, and the EXPECT strings in this directory's CMakeLists.txt.
//
// Expected guard: "fx literal overflow" (inside detail::parse_u64)
#include <moba/fx/fx.hpp>

using namespace moba;

int main() {
  constexpr fx v = 1000000000000000000000_fx;
  return v.raw;
}
