# MobaWarnings.cmake
#
# moba_apply_compile_settings(<interface-target>)
#
# Every warning and codegen flag the project depends on lives here. Anything
# that changes generated code for the simulation belongs in this file and
# nowhere else -- a flag that differs between two targets is a desync waiting
# to happen.

function(moba_apply_compile_settings target)
    # --- Warnings ----------------------------------------------------------
    #
    # Deliberately NOT here: -Wpedantic. It rejects __int128, which fx64
    # requires, and -Werror turns that into a build failure.
    set(_warnings
        -Wall
        -Wextra
        -Wconversion # silent narrowing is how fixed-point maths dies
        -Wsign-conversion
        -Wshadow
        -Wold-style-cast # this is C++, not C
        -Wdouble-promotion # catches accidental float in the simulation
        -Wnon-virtual-dtor
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wnull-dereference
    )

    if(MOBA_WERROR)
        list(APPEND _warnings -Werror)
    endif()

    # --- Codegen -----------------------------------------------------------
    #
    # Determinism-relevant. Do not change without regenerating golden hashes.
    set(_codegen
        -fwrapv # signed overflow wraps, defined, identical everywhere
        -ffp-contract=off # no FMA contraction; keeps the double reference
        # comparison meaningful across compilers
    )

    # --- Reproducible paths -------------------------------------------------
    #
    # __FILE__ expands to whatever path the compiler was handed, which under
    # CMake is absolute -- so MOBA_ASSERT output leaks the developer's home
    # directory and differs between machines. This rewrites the source-root
    # prefix away, leaving repo-relative paths.
    #
    # -fmacro-prefix-map, not -ffile-prefix-map: this rewrites __FILE__ only
    # and leaves DWARF paths absolute, so the debugger still finds sources.
    # Switch to -ffile-prefix-map if you later want bit-reproducible binaries,
    # and expect to run the debugger from the source root.
    set(_paths -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

    target_compile_options(
        ${target}
        INTERFACE ${_warnings} ${_codegen} ${_paths}
    )

    # --- Sanitizers --------------------------------------------------------
    #
    # What is on, and why each one earns its place.
    if(MOBA_SANITIZE)
        # address  -- heap/stack/global overflow, use-after-free,
        #             use-after-return, use-after-scope.
        # undefined -- the UBSan group. The members that matter here:
        #     integer-divide-by-zero  (fx::operator/ has no guard yet)
        #     shift                   (fixed point is made of shifts; a shift
        #                              count >= width or a shift of a negative
        #                              value is UB and silently differs
        #                              between compilers, which is a desync)
        #     array-bounds, alignment, null, return, bool, enum,
        #     implicit truncation on function returns, vptr
        set(_san
            -fsanitize=address,undefined
            -fno-sanitize-recover=all # UB aborts. Without this UBSan prints
            # and continues, and ctest goes green
            # on a test that invoked UB.
            -fno-omit-frame-pointer
            -g # symbolised stack traces even in
            # optimised sanitizer builds
        )

        # local-bounds catches out-of-range indexing of fixed-size local
        # arrays, which is precisely what fixed_vector, slot_map, ring_buffer
        # and the trig LUTs are made of. Not part of the `undefined` group,
        # and Clang-only.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            list(APPEND _san -fsanitize=local-bounds)
        endif()

        target_compile_options(${target} INTERFACE ${_san})
        target_link_options(${target} INTERFACE ${_san})

        # Hardened standard library. Turns std::span, std::array and the
        # container operator[]s into checked accesses. Sanitizers cannot see
        # these on their own -- an in-bounds-for-the-allocation but
        # out-of-range-for-the-container index is invisible to ASan.
        #
        # Each macro is ignored by the standard library that does not own it,
        # so both can be defined unconditionally.
        target_compile_definitions(
            ${target}
            INTERFACE
                _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG # libc++ >= 18
                _GLIBCXX_ASSERTIONS # libstdc++
        )
    endif()

    # --- Deliberately NOT enabled -------------------------------------------
    #
    # unsigned-integer-overflow  angle is a u16 where wraparound IS the
    #                            semantics. Fires on correct code every frame.
    # implicit-conversion        Fires on every deliberate narrowing in fx.
    #                            -Wconversion covers the accidental ones.
    # signed-integer-overflow    Suppressed by -fwrapv, by design. Consequence:
    #                            UBSan will NOT catch fx overflow. The debug
    #                            asserts inside fx are the only net. Write them.
    # MemorySanitizer            Unsupported on Darwin; needs an instrumented
    #                            stdlib.
    # ThreadSanitizer            No threads by design, and cannot combine with
    #                            ASan. Its own preset if platform ever threads.
    # float-divide-by-zero       No floating point in the sim.
endfunction()
