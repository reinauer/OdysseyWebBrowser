list(APPEND WTF_SOURCES
    posix/ThreadingPOSIX.cpp
    posix/FileSystemPOSIX.cpp
    generic/WorkQueueGeneric.cpp
    generic/RunLoopGeneric.cpp
    text/mui/TextBreakIteratorInternalICUMorphOS.cpp
    mui/CPUTimeAROS.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/MainThreadGeneric.cpp
)
if (AmigaOS3)
    list(APPEND WTF_SOURCES amigaos3/LanguageAmigaOS3.cpp)
else ()
    list(APPEND WTF_SOURCES mui/LanguageMorphOS.cpp)
endif ()
if (AROS)
    list(APPEND WTF_SOURCES 
        mui/execallocator.cpp
        OSAllocatorAROS.cpp
        generic/MemoryPressureHandlerGeneric.cpp
    )
endif()
if (AmigaOS4)
    list(APPEND WTF_SOURCES 
        OSAllocatorAmigaOS.cpp
        mui/OS4Misc.cpp
        generic/MemoryPressureHandlerGeneric.cpp
#        mui/MemoryPressureHandlerMorphOS.cpp
    )
endif()
if (AmigaOS3)
    list(APPEND WTF_SOURCES
        OSAllocatorAmigaOS3.cpp
        amigaos3/ThreadContext.cpp
        amigaos3/Random.c
        amigaos3/Time.c
        generic/MemoryPressureHandlerGeneric.cpp
    )
endif()
