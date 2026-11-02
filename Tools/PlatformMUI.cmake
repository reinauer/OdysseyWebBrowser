add_subdirectory(OdysseyWebBrowser)

if (AmigaOS3)
    add_executable(amigaos3-math-test EXCLUDE_FROM_ALL AmigaOS3/MathTest.cpp
        ${WTF_DIR}/wtf/amigaos3/Time.c)
    target_link_libraries(amigaos3-math-test PRIVATE AmigaOS3::Support)

    add_executable(amigaos3-persistence-test EXCLUDE_FROM_ALL
        AmigaOS3/persistence-test.cpp)
    target_include_directories(amigaos3-persistence-test PRIVATE
        "${WTF_DIR}" "$<TARGET_PROPERTY:WTF,INCLUDE_DIRECTORIES>")
    target_link_libraries(amigaos3-persistence-test PRIVATE WTF AmigaOS3::Support)

    add_executable(amigaos3-keyboard-test EXCLUDE_FROM_ALL
        AmigaOS3/KeyboardTest.cpp
        ${WEBCORE_DIR}/platform/mui/PlatformWheelEventMorphOS.cpp
        ${WEBCORE_DIR}/platform/mui/PlatformKeyboardEventAmigaOS3.cpp
        ${WEBCORE_DIR}/platform/mui/LocalEncodingAmigaOS3.cpp)
    target_include_directories(amigaos3-keyboard-test PRIVATE
        "${WEBCORE_DIR}" "${WEBCORE_DIR}/platform" "${WEBCORE_DIR}/platform/mui"
        "${WEBCORE_DIR}/PAL"
        "${CMAKE_BINARY_DIR}/JavaScriptCore/PrivateHeaders"
        "$<TARGET_PROPERTY:WebCore,INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:WTF,INCLUDE_DIRECTORIES>")
    target_link_libraries(amigaos3-keyboard-test PRIVATE WTF ICU::uc AmigaOS3::Support)
    add_dependencies(amigaos3-keyboard-test JavaScriptCore_CopyPrivateHeaders)

    # Stage the current OWB and pack it for extraction with native LhA.
    set(AMIGAOS3_NEWLIB_LICENSE "${CMAKE_SOURCE_DIR}/build/amigaos3-browser-support/COPYING.NEWLIB"
        CACHE FILEPATH "newlib notice for the installation bundle")
    set(AMIGAOS3_PTHREAD_SOURCES "${CMAKE_SOURCE_DIR}/build/amigaos3-gthreads-oct29/pthread-source/pthreads"
        CACHE PATH "pthread sources providing the bundle's notices")
    set(_amigaos3_lha "${CMAKE_BINARY_DIR}/Odyssey-AmigaOS3.lha")
    add_custom_target(lha
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}/lha-stage"
            "${_amigaos3_lha}" "${_amigaos3_lha}.sha256"
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/AmigaOS3/stage-install.py"
            --binary "$<TARGET_FILE:OWB>"
            --source-root "${CMAKE_SOURCE_DIR}"
            --dependency-root "${AMIGAOS3_DEPENDENCY_ROOT}"
            --runtime-root "${AMIGAOS3_RUNTIME_ROOT}"
            --pthread-sources "${AMIGAOS3_PTHREAD_SOURCES}"
            --newlib-license "${AMIGAOS3_NEWLIB_LICENSE}"
            --output "${CMAKE_BINARY_DIR}/lha-stage"
            --lha-archive "${_amigaos3_lha}"
        DEPENDS OWB
        VERBATIM USES_TERMINAL)
endif ()

