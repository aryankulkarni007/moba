#pragma once

// moba/fx/isqrt.hpp -- integer square root. SPEC ONLY, not implemented.
//
// TODO: [missing] Whole file. Callers: vec2::length, vec2::normalise. If a
//       third appears, check whether it wants a squared comparison instead.
//
//   isqrt(u32) -> u32    floor(sqrt(n)), exact
//   sqrt(fx) -> fx
//
// Notes:
//   - Newton, or the restoring bit-by-bit algorithm. Bit-by-bit has no
//     division; Newton needs a starting estimate (std::bit_width, <bit>).
//   - The loop must run a fixed number of iterations, or terminate on a
//     condition depending only on the input. A convergence test that could
//     differ between compilers is a desync.
//   - Postcondition is the test: r*r <= n && (r+1)*(r+1) > n.
//     Check it over sampled values plus every power-of-two boundary.
//   - Exhaustive 2^32 verification is feasible but does not belong in ctest.
//     Separate tool, run once, record the result.
