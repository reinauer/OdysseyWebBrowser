#ifndef ODYSSEY_AMIGAOS3OPENURL_H
#define ODYSSEY_AMIGAOS3OPENURL_H

#include <exec/libraries.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <inline/macros.h>

// The public OpenURL SFD defines URL_OpenA at -30, with URL in a0 and
// tags in a1. Keep this optional binding local instead of depending on
// an autoinit archive or an SDK for another Amiga platform.
static inline bool amigaOS3OpenURL(const char* url)
{
    if (!url)
        return false;
    struct Library* openURLBase = OpenLibrary("openurl.library", 0);
    if (!openURLBase)
        return false;
    struct TagItem tags[] = { { TAG_DONE, 0 } };
    ULONG result = LP2(0x1e, ULONG, URL_OpenA,
        STRPTR, const_cast<char*>(url), a0, struct TagItem*, tags, a1,
        , openURLBase);
    CloseLibrary(openURLBase);
    return result != 0;
}

#endif
