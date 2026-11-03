# Classic AmigaOS, using the compiler's own SDK (including MUI headers).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)
set(AmigaOS3 ON CACHE BOOL "Build for classic AmigaOS" FORCE)
set(AMIGAOS3_TOOLCHAIN_ROOT "/opt/amiga-16.2" CACHE PATH "Amiga GCC installation")
# Every object in the link, including the runtime overlay and dependencies,
# must use the same flags: with -m68881 floating-point results are returned
# in fp0. "-m68020" selects the earlier software floating point profile.
set(AMIGAOS3_CPU_FLAGS "-m68020-60 -m68881" CACHE STRING "Code generation flags for every object")
if (AMIGAOS3_CPU_FLAGS MATCHES "-m68881")
    set(AMIGAOS3_SOFT_FLOAT OFF)
else ()
    set(AMIGAOS3_SOFT_FLOAT ON)
endif ()
set(AMIGAOS3_DEPENDENCY_ROOT "" CACHE PATH "Additional m68k dependency prefix")
set(AMIGAOS3_RUNTIME_ROOT "" CACHE PATH "Pthread-enabled GCC runtime overlay")
set(CMAKE_C_COMPILER "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-gcc")
set(CMAKE_CXX_COMPILER "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-g++")
set(CMAKE_AR "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-ar")
set(CMAKE_RANLIB "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-ranlib")
set(CMAKE_C_FLAGS_INIT "${AMIGAOS3_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${AMIGAOS3_CPU_FLAGS}")
# -O3 made the code about 20% larger than -O2; the code hunk is loaded whole.
# CMake appends its own -O3 to a *_INIT value, so set the cache entries.
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the C compiler for Release builds")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the C++ compiler for Release builds")
if (AMIGAOS3_RUNTIME_ROOT)
    set(_runtime_files include/c++/m68k-amigaos/bits/c++config.h
            include/c++/bits/amigaos3_c99_math.h
            lib/libstdc++.a lib/libsupc++.a lib/libnew_noexc.a
            lib/libgcc.a lib/libpthread.a)
    if (AMIGAOS3_SOFT_FLOAT)
        list(APPEND _runtime_files lib/floatunsidf.o lib/floatunsisf.o
            lib/truncdfsf2.o lib/double-math.o)
    endif ()
    foreach (_runtime_file ${_runtime_files})
        if (NOT EXISTS "${AMIGAOS3_RUNTIME_ROOT}/${_runtime_file}")
            message(FATAL_ERROR "Incomplete AmigaOS3 runtime: ${_runtime_file}")
        endif ()
    endforeach ()
    string(APPEND CMAKE_C_FLAGS_INIT " -isystem ${AMIGAOS3_RUNTIME_ROOT}/include")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdinc++ -isystem ${AMIGAOS3_RUNTIME_ROOT}/include/c++ -isystem ${AMIGAOS3_RUNTIME_ROOT}/include/c++/m68k-amigaos -isystem ${AMIGAOS3_RUNTIME_ROOT}/include")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-L${AMIGAOS3_RUNTIME_ROOT}/lib")
endif ()
set(CMAKE_FIND_ROOT_PATH "${AMIGAOS3_TOOLCHAIN_ROOT}/m68k-amigaos")
if (AMIGAOS3_DEPENDENCY_ROOT)
    list(PREPEND CMAKE_FIND_ROOT_PATH "${AMIGAOS3_DEPENDENCY_ROOT}")
endif ()
if (AMIGAOS3_RUNTIME_ROOT)
    # pkg-config dependencies are resolved to absolute archives by CMake.
    # A linker -L flag alone cannot keep them out of the SDK's pthread
    # archive, which must not be mixed with the selected C++ runtime.
    list(PREPEND CMAKE_FIND_ROOT_PATH "${AMIGAOS3_RUNTIME_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${AMIGAOS3_RUNTIME_ROOT}")
endif ()
# GCC picks the SDK's libraries for these flags from a multilib directory,
# but CMake resolves pkg-config's "-lm" to an absolute path and would take
# m68k-amigaos/lib/libm.a, the 68000 soft-float build, which returns doubles
# in d0/d1 instead of fp0. Search the multilib directory before lib. CMake
# tries each search path under every root in turn, so the port's own
# prefixes come first; the SDK's multilib copies of zlib, libpng and
# FreeType must not replace the dependency builds.
separate_arguments(_amigaos3_cpu_flags UNIX_COMMAND "${AMIGAOS3_CPU_FLAGS}")
execute_process(COMMAND "${CMAKE_C_COMPILER}" ${_amigaos3_cpu_flags} -print-multi-directory
    OUTPUT_VARIABLE _amigaos3_multilib OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _amigaos3_multilib_result)
if (NOT _amigaos3_multilib_result EQUAL 0 OR _amigaos3_multilib STREQUAL "")
    message(FATAL_ERROR "Cannot determine the multilib directory for ${AMIGAOS3_CPU_FLAGS}")
endif ()
if (NOT _amigaos3_multilib STREQUAL ".")
    set(_amigaos3_library_path)
    if (AMIGAOS3_RUNTIME_ROOT)
        list(APPEND _amigaos3_library_path "${AMIGAOS3_RUNTIME_ROOT}/lib")
    endif ()
    if (AMIGAOS3_DEPENDENCY_ROOT)
        list(APPEND _amigaos3_library_path "${AMIGAOS3_DEPENDENCY_ROOT}/lib")
    endif ()
    list(APPEND _amigaos3_library_path
        "${AMIGAOS3_TOOLCHAIN_ROOT}/m68k-amigaos/lib/${_amigaos3_multilib}")
    list(PREPEND CMAKE_LIBRARY_PATH ${_amigaos3_library_path})
endif ()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    AMIGAOS3_TOOLCHAIN_ROOT AMIGAOS3_CPU_FLAGS AMIGAOS3_DEPENDENCY_ROOT
    AMIGAOS3_RUNTIME_ROOT)
