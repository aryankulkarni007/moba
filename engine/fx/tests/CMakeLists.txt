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
