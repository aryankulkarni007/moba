// Shared doctest entry point.
//
// Every moba test binary links this object library, so no individual test
// translation unit defines its own main(). Adding a new test file means adding
// it to a moba_add_test(SOURCES ...) list and nothing else.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
