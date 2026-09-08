moba_add_test(test_fx
    SOURCES
        test_fx.cpp
        test_fx64.cpp
        test_isqrt.cpp
        test_isqrt.cpp
    LIBS
        moba::fx
)

# The _fx literal is consteval, so every rejection it performs is a COMPILE
# error. Those cannot be expressed as runtime assertions -- each one is a TU
# that must fail to build.
#
# EXPECT is a substring of the `throw` message in <moba/fx/fx.hpp> that the
# input is supposed to trip. Without it the test would pass on any compile
# error at all; see the note above moba_add_compile_fail_test in MobaTest.cmake.
# Change a message there and these must change with it -- that coupling is
# intentional, it is what stops the tests going vacuous.

# 32768_fx -- one past fx::MAX_INT
moba_add_compile_fail_test(fx_lit_int_overflow
    SOURCE compile_fail/lit_int_overflow.cpp
    EXPECT "integer part overflow"
    LIBS   moba::fx
)

# 100000_fx -- far past fx::MAX_INT, same guard
moba_add_compile_fail_test(fx_lit_int_overflow2
    SOURCE compile_fail/lit_int_overflow2.cpp
    EXPECT "integer part overflow"
    LIBS   moba::fx
)

# The next three all trip the trailing-characters guard: parsing stops at the
# first character that is not a digit or the single '.', and anything left over
# is a rejection.
#
#   1e3_fx      exponent form
#   0x10_fx     hex
#   1'000_fx    digit separator
moba_add_compile_fail_test(fx_lit_exponent
    SOURCE compile_fail/lit_exponent.cpp
    EXPECT "only decimal digits"
    LIBS   moba::fx
)

moba_add_compile_fail_test(fx_lit_hex
    SOURCE compile_fail/lit_hex.cpp
    EXPECT "only decimal digits"
    LIBS   moba::fx
)

moba_add_compile_fail_test(fx_lit_digit_sep
    SOURCE compile_fail/lit_digit_sep.cpp
    EXPECT "only decimal digits"
    LIBS   moba::fx
)

# 10^21 -- overflows the u64 accumulator inside parse_u64 itself, before the
# fx range check is ever reached. A different guard from the two above.
moba_add_compile_fail_test(fx_lit_parse_overflow
    SOURCE compile_fail/lit_parse_overflow.cpp
    EXPECT "fx literal overflow"
    LIBS   moba::fx
)
