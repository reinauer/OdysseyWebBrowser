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
endif ()

