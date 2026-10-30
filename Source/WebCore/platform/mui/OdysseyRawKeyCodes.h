#pragma once

#include <wtf/Platform.h>

#if OS(AMIGAOS3)
#include <libraries/keymap.h>

// Classic SDK spelling aliases. Numeric keypad positions are the classic
// layout also documented by zune68's gcc/devices/rawkeycodes.h.
#define RAWKEY_ESCAPE RAWKEY_ESC
#define RAWKEY_UP RAWKEY_CRSRUP
#define RAWKEY_DOWN RAWKEY_CRSRDOWN
#define RAWKEY_LEFT RAWKEY_CRSRLEFT
#define RAWKEY_RIGHT RAWKEY_CRSRRIGHT
#define RAWKEY_NM_WHEEL_UP RAWKEY_WHEEL_UP
#define RAWKEY_NM_WHEEL_DOWN RAWKEY_WHEEL_DOWN
#define RAWKEY_NM_WHEEL_LEFT RAWKEY_WHEEL_LEFT
#define RAWKEY_NM_WHEEL_RIGHT RAWKEY_WHEEL_RIGHT
#define RAWKEY_KP_0 0x0f
#define RAWKEY_KP_1 0x1d
#define RAWKEY_KP_2 0x1e
#define RAWKEY_KP_3 0x1f
#define RAWKEY_KP_4 0x2d
#define RAWKEY_KP_5 0x2e
#define RAWKEY_KP_6 0x2f
#define RAWKEY_KP_7 0x3d
#define RAWKEY_KP_8 0x3e
#define RAWKEY_KP_9 0x3f
#else
#include <devices/rawkeycodes.h>
#endif
