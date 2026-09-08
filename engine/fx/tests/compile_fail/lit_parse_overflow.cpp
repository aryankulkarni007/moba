// MUST NOT COMPILE -- trips parse_u64's own u64 guard.
//
// Driven by moba_add_compile_fail_test() with WILL_FAIL, so a successful build
// here is a test failure. See <moba/fx/fx.hpp>, THE _fx LITERAL.
#include <moba/fx/fx.hpp>

using namespace moba;

int main() {
  constexpr fx v = 1000000000000000000000_fx;
  return v.raw;
}
