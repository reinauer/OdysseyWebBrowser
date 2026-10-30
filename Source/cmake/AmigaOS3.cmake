# Classic AmigaOS, using the compiler's own SDK (including MUI headers).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)
set(AmigaOS3 ON CACHE BOOL "Build for classic AmigaOS" FORCE)
set(AMIGAOS3_TOOLCHAIN_ROOT "/opt/amiga-16.2" CACHE PATH "Amiga GCC installation")
set(AMIGAOS3_CPU "68020" CACHE STRING "Minimum Motorola CPU")
set(AMIGAOS3_DEPENDENCY_ROOT "" CACHE PATH "Additional m68k dependency prefix")
set(AMIGAOS3_RUNTIME_ROOT "" CACHE PATH "Pthread-enabled GCC runtime overlay")
set(CMAKE_C_COMPILER "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-gcc")
set(CMAKE_CXX_COMPILER "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-g++")
set(CMAKE_AR "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-ar")
set(CMAKE_RANLIB "${AMIGAOS3_TOOLCHAIN_ROOT}/bin/m68k-amigaos-ranlib")
set(CMAKE_C_FLAGS_INIT "-m${AMIGAOS3_CPU}")
set(CMAKE_CXX_FLAGS_INIT "-m${AMIGAOS3_CPU}")
if (AMIGAOS3_RUNTIME_ROOT)
    foreach (_runtime_file include/c++/m68k-amigaos/bits/c++config.h
            include/c++/bits/amigaos3_c99_math.h
            lib/libstdc++.a lib/libsupc++.a lib/libnew_noexc.a
            lib/libgcc.a lib/libpthread.a lib/floatunsidf.o lib/floatunsisf.o
            lib/truncdfsf2.o lib/double-math.o)
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
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    AMIGAOS3_TOOLCHAIN_ROOT AMIGAOS3_CPU AMIGAOS3_DEPENDENCY_ROOT
    AMIGAOS3_RUNTIME_ROOT)
