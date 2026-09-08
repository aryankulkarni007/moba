moba_add_test(test_assert
    SOURCES test_assert.cpp
    LIBS moba::core
)

moba_add_test(test_types
    SOURCES test_types.cpp
    LIBS moba::core
)

moba_add_test(test_strong_id
    SOURCES test_strong_id.cpp
    LIBS moba::core
)
