list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp
    text/KillRing.cpp
    system/mui/SoundMorphOS.cpp
    system/ClockGeneric.cpp
)
if (AmigaOS3)
    list(APPEND PAL_LIBRARIES AmigaOS3::AmiSSL)
endif ()
