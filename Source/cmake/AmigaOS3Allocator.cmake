# The SDK MemMap allocator bypasses newlib's malloc-lock hooks. Keep these
# wrappers as an object target so they precede libc even for static builds.
# The descriptor wrapper also preserves real terminal detection when SDK
# open.o supplies a public isatty that only recognizes descriptor numbers.
if (NOT TARGET AmigaOS3Allocator)
    add_library(AmigaOS3Allocator OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/../WTF/wtf/amigaos3/Malloc.c"
        "${CMAKE_CURRENT_LIST_DIR}/../WTF/wtf/amigaos3/TTY.c")
    add_link_options(
        -Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r,--wrap=isatty)
    link_libraries("$<TARGET_OBJECTS:AmigaOS3Allocator>")
    # These compiler helpers must precede libc. Oct14 had an incorrect
    # unsigned high-bit constant, fixed in Oct29; both profiles retain the
    # tested GCC conversions plus float rounding and IEEE double arithmetic.
    add_library(AmigaOS3CompilerHelpers OBJECT IMPORTED GLOBAL)
    set(_amigaos3_compiler_objects
        "${AMIGAOS3_RUNTIME_ROOT}/lib/floatunsidf.o"
        "${AMIGAOS3_RUNTIME_ROOT}/lib/floatunsisf.o"
        "${AMIGAOS3_RUNTIME_ROOT}/lib/truncdfsf2.o"
        "${AMIGAOS3_RUNTIME_ROOT}/lib/double-math.o")
    if (EXISTS "${AMIGAOS3_RUNTIME_ROOT}/lib/integer-math.o")
        # Avoid libc's signed-division object, which also defines div/ldiv
        # and collides with its separate standard-library implementations.
        list(APPEND _amigaos3_compiler_objects
            "${AMIGAOS3_RUNTIME_ROOT}/lib/integer-math.o")
    endif ()
    set_target_properties(AmigaOS3CompilerHelpers PROPERTIES
        IMPORTED_OBJECTS "${_amigaos3_compiler_objects}")
    link_libraries("$<TARGET_OBJECTS:AmigaOS3CompilerHelpers>")
endif ()
