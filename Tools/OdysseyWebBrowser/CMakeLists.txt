set(OdysseyWebBrowser_SOURCES
    main.cpp
)

set(OdysseyWebBrowser_INCLUDE_DIRECTORIES
    "${CAIRO_INCLUDE_DIRS}"
    "${WEBKITLEGACY_DIR}/mui/UI"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
)

if (NOT AmigaOS3)
    list(APPEND OdysseyWebBrowser_INCLUDE_DIRECTORIES
        "${WEBKITLEGACY_DIR}/mui/UI/AROS/include")
endif()

if (AmigaOS3)
list(APPEND OdysseyWebBrowser_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/network/curl")
# Resolve the native static dependency graph through the framework targets.
# Optional media/image dependencies follow WebCore's enabled features.
set(OdysseyWebBrowser_LIBRARIES
    -Wl,--start-group
    WebKitLegacy
    WebCore
    JavaScriptCore
    -Wl,--end-group
    -ldebug
)
elseif (AmigaOS4)
set(OdysseyWebBrowser_LIBRARIES
    JavaScriptCore
    WebKitLegacy
    -mcrt=clib4
    -Wl,--start-group WebCore -Wl,--end-group
    -lfreetype
    -lcairo
    -lwebp 
    -lwebpdemux
    -lfontconfig
    -lbrotlidec
    -lbrotlicommon
    -lz
    -lcurl
    -lrtmp
    -lidn2
    -ljpeg
    -lpng
    -lssl
    -lcrypto
    -licui18n
    -licuuc
    -licudata
    -lsqlite3
    -lxslt
    -lxml2
    -lexpat
    -lavformat
    -lbz2
    -lavcodec
    -lz
    -lavutil
    -lswresample
    -lswscale
    -lopencore-amrwb 
    -lopencore-amrnb 
    -lmp3lame 
    -lopenjp2 
    -lvpx 
    -lvorbisfile 
    -lvorbisenc 
    -lvorbis 
    -logg 
    -lopus 
    -ltheoraenc 
    -ltheoradec 
    -lwebp 
    -lsharpyuv 
    -lxvidcore 
    -lpsl
    -latomic
    -lpixman-1
    -lfontconfig
    -lnghttp2
    -llzma
    -lzstd
    -lpthread
    -lunistring
    ${ZLIB_LIBRARIES}    
    -athread=native
)
else()
set(OdysseyWebBrowser_LIBRARIES
    JavaScriptCore
    WebKitLegacy
    -Wl,--start-group WebCore -Wl,--end-group
    -lz
    -lcairo
    -lpixman
    -lfontconfig
    -lfreetype2
    -lxml2
    -lz
    -lcurl
    -ljpeg
    -lpng
    -lssl
    -lcrypto
    -licui18n
    -licuuc
    -licudata
    -lsqlite3
    -lxslt
    -ldebug
    -lopenurl
    -lrexxsyslib
    -lavformat
    -lbz2
    -lavcodec
    -liconv
    -lz
    -lavutil
    -lswresample
    -lswscale
    -lpsl
    -latomic
    ${ZLIB_LIBRARIES}
)
endif()

include_directories(${OdysseyWebBrowser_INCLUDE_DIRECTORIES})
if (AmigaOS4)
add_executable(Odyssey ${OdysseyWebBrowser_SOURCES})
target_link_libraries(Odyssey ${OdysseyWebBrowser_LIBRARIES})
else()
add_executable(OWB ${OdysseyWebBrowser_SOURCES})
target_link_libraries(OWB ${OdysseyWebBrowser_LIBRARIES})
endif()
