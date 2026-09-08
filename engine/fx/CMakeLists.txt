# moba::fx -- Q16.16 / Q32.32 fixed point. Angles, 2D vectors and shape
# predicates are specified in their headers but not yet written.
#
# Header-only (INTERFACE) so every operation can inline into the tick. Never
# SHARED: that would block cross-TU inlining and risk a flag mismatch on the
# hot path, and a flag mismatch is a desync.

add_library(moba_fx INTERFACE)
add_library(moba::fx ALIAS moba_fx)

target_include_directories(moba_fx INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# fx depends on core, never the reverse.
target_link_libraries(moba_fx INTERFACE
    moba::core
    moba::options
)

target_sources(moba_fx INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/fx.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/fx64.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/angle.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/vec2.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/shapes.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/isqrt.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/fx/format.hpp
)

if(MOBA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
