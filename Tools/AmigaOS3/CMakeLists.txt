cmake_minimum_required(VERSION 3.16)
project(OdysseyAmigaOS3Platform LANGUAGES C CXX)

if (NOT AmigaOS3)
    message(FATAL_ERROR "Use Source/cmake/AmigaOS3.cmake as the toolchain")
endif ()
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()
get_filename_component(ODYSSEY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile production sources, without pulling in the browser's ICU, Cairo
# and networking dependencies. This is a platform test, not a browser shell.
add_library(odyssey-amigaos3-platform STATIC
    ${ODYSSEY_ROOT}/Source/WTF/wtf/CurrentTime.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/FastMalloc.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/OSAllocatorAmigaOS3.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/PageBlock.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/StackBounds.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/StackPointer.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/amigaos3/Time.c
)
target_include_directories(odyssey-amigaos3-platform PUBLIC
    ${ODYSSEY_ROOT}/Source/WTF)
target_compile_options(odyssey-amigaos3-platform PUBLIC
    -include wtf/Platform.h
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
target_compile_definitions(odyssey-amigaos3-platform PUBLIC NDEBUG)
add_executable(odyssey-platform-test PlatformTest.cpp)
target_link_libraries(odyssey-platform-test PRIVATE
    pthread odyssey-amigaos3-platform)
