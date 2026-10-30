# The SDK MemMap allocator bypasses newlib's malloc-lock hooks. Keep these
# wrappers as an object target so they precede libc even for static builds.
if (NOT TARGET AmigaOS3Allocator)
    add_library(AmigaOS3Allocator OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/../WTF/wtf/amigaos3/Malloc.c")
    add_link_options(
        -Wl,--wrap=_malloc_r,--wrap=_free_r,--wrap=_calloc_r,--wrap=_realloc_r)
    link_libraries("$<TARGET_OBJECTS:AmigaOS3Allocator>")
    # These compiler helpers must precede libc, whose Amiga wrappers have an
    # incorrect high-bit constant, float rounding and non-IEEE double
    # arithmetic. Use the matching GCC software implementations.
    add_library(AmigaOS3CompilerHelpers OBJECT IMPORTED GLOBAL)
    set_target_properties(AmigaOS3CompilerHelpers PROPERTIES IMPORTED_OBJECTS
        "${AMIGAOS3_RUNTIME_ROOT}/lib/floatunsidf.o;${AMIGAOS3_RUNTIME_ROOT}/lib/floatunsisf.o;${AMIGAOS3_RUNTIME_ROOT}/lib/truncdfsf2.o;${AMIGAOS3_RUNTIME_ROOT}/lib/double-math.o")
    link_libraries("$<TARGET_OBJECTS:AmigaOS3CompilerHelpers>")
endif ()
