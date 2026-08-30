# engine/
#
# Game-agnostic libraries
#
# Order matters: a library may only depend on ones listed above it
#
#   core      types, assert, fixed-capacity containers
#   fx        fixed-point scalars, angles, vectors, shapes
#   util      hashing, PCG32, deterministic trace log      (not yet)
#   platform  clocks and timing -- isolated so the sim target
#             can refuse to link it and therefore cannot read a clock  (not yet)

add_subdirectory(core)
add_subdirectory(fx)
