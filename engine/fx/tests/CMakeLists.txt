moba_add_test(test_fx
    SOURCES
        test_fx.cpp
        # TODO: test_fx64.cpp   -- mul_wide exactness, narrow() saturation,
        #                          rounding agreement with fx::operator* on negatives
        # TODO: test_golden.cpp -- the ~100k mixed-operation hash that must be
        #                          identical under all three presets
    LIBS
        moba::fx
)

# The _fx literal is consteval, so every rejection it performs is a COMPILE
# error. Those cannot be expressed as runtime assertions -- each one is a TU
# that must fail to build, driven with WILL_FAIL.
moba_add_compile_fail_test(fx_lit_int_overflow
    compile_fail/lit_int_overflow.cpp moba::fx)
moba_add_compile_fail_test(fx_lit_int_overflow2
    compile_fail/lit_int_overflow2.cpp moba::fx)
moba_add_compile_fail_test(fx_lit_exponent
    compile_fail/lit_exponent.cpp moba::fx)
moba_add_compile_fail_test(fx_lit_hex
    compile_fail/lit_hex.cpp moba::fx)
moba_add_compile_fail_test(fx_lit_digit_sep
    compile_fail/lit_digit_sep.cpp moba::fx)
moba_add_compile_fail_test(fx_lit_parse_overflow
    compile_fail/lit_parse_overflow.cpp moba::fx)
