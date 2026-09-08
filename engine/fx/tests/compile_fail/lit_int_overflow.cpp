// MUST NOT COMPILE -- integer part is 32768; fx::MAX_INT is 32767.
//
// Driven by moba_add_compile_fail_test() with WILL_FAIL, so a successful build
// here is a test failure. See <moba/fx/fx.hpp>, THE _fx LITERAL.
#include <moba/fx/fx.hpp>

using namespace moba;

int main() {
  constexpr fx v = 32768_fx;
  return v.raw;
}
