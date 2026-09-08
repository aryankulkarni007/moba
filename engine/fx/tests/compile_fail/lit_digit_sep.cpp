// MUST NOT COMPILE -- digit separators are deliberately not supported.
//
// Driven by moba_add_compile_fail_test() with WILL_FAIL, so a successful build
// here is a test failure. See <moba/fx/fx.hpp>, THE _fx LITERAL.
#include <moba/fx/fx.hpp>

using namespace moba;

int main() {
  constexpr fx v = 1'000_fx;
  return v.raw;
}
