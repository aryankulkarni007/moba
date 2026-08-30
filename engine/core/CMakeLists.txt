# moba::core -- fundamental types, assertions, fixed-capacity containers.
#
# Header-only (INTERFACE): every consumer -- sim, net, platform -- wants the
# integer aliases without dragging in a maths library, and INTERFACE keeps this
# free at link time.

add_library(moba_core INTERFACE)
add_library(moba::core ALIAS moba_core)

target_include_directories(moba_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(moba_core INTERFACE moba::options)

# Headers listed so they appear in IDE project trees.
target_sources(moba_core INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/core/types.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/core/assert.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/core/strong_id.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/moba/core/result.hpp
)

if(MOBA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
