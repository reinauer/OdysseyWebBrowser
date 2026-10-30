cmake_minimum_required(VERSION 3.16)
project(OdysseyAmigaOS3Platform LANGUAGES C CXX)

if (NOT AmigaOS3)
    message(FATAL_ERROR "Use Source/cmake/AmigaOS3.cmake as the toolchain")
endif ()
if (NOT AMIGAOS3_RUNTIME_ROOT)
    message(FATAL_ERROR "Set AMIGAOS3_RUNTIME_ROOT to the pthread-enabled runtime prefix")
endif ()
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()
get_filename_component(ODYSSEY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
include(${ODYSSEY_ROOT}/Source/cmake/AmigaOS3Allocator.cmake)
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
    ${ODYSSEY_ROOT}/Source/WTF/wtf/amigaos3/ThreadContext.cpp
)
target_include_directories(odyssey-amigaos3-platform PUBLIC
    ${ODYSSEY_ROOT}/Source/WTF)
target_compile_options(odyssey-amigaos3-platform PUBLIC
    -include wtf/Platform.h
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
target_compile_definitions(odyssey-amigaos3-platform PUBLIC NDEBUG)
add_executable(odyssey-platform-test PlatformTest.cpp ThreadContextTest.cpp MUICallbackTest.cpp ClipboardNotificationTest.cpp)
target_include_directories(odyssey-platform-test PRIVATE
    ${ODYSSEY_ROOT}/Source/WebKitLegacy/mui/UI)
target_link_libraries(odyssey-platform-test PRIVATE
    pthread odyssey-amigaos3-platform)

add_executable(odyssey-random-test RandomTest.cpp
    ${ODYSSEY_ROOT}/Source/WTF/wtf/amigaos3/Random.c)
target_link_libraries(odyssey-random-test PRIVATE
    pthread odyssey-amigaos3-platform)

add_executable(odyssey-runtime-test RuntimeTest.cpp)
target_link_libraries(odyssey-runtime-test PRIVATE
    pthread odyssey-amigaos3-platform atomic)

add_executable(odyssey-saturation-test SaturatedArithmeticTest.cpp)
target_link_libraries(odyssey-saturation-test PRIVATE
    pthread odyssey-amigaos3-platform)

add_executable(odyssey-math-test MathTest.cpp)
target_link_libraries(odyssey-math-test PRIVATE
    pthread odyssey-amigaos3-platform atomic)

add_executable(odyssey-graphics-test GraphicsTest.cpp
    ${ODYSSEY_ROOT}/Source/WebKitLegacy/mui/UI/amigaos3graphics.cpp)
target_include_directories(odyssey-graphics-test PRIVATE
    ${ODYSSEY_ROOT}/Source/WebKitLegacy/mui/UI)
target_link_libraries(odyssey-graphics-test PRIVATE
    pthread odyssey-amigaos3-platform)
