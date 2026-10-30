#ifndef ODYSSEY_AMIGAOS3FUNCS_H
#define ODYSSEY_AMIGAOS3FUNCS_H

#include <clib/alib_protos.h>
#include <intuition/classes.h>
#include <stdarg.h>
#include <stdlib.h>
#include "include/macros/compilers.h"

typedef ULONG IPTR;
typedef LONG SIPTR;
typedef ULONG STACKIPTR;
typedef long long QUAD;
typedef unsigned long long UQUAD;
// Native m68k message fields already use the stack's word alignment.
#define STACKED

// Cairo ARGB32 bytes on big-endian m68k match CyberGraphX's ARGB format.
#define NATIVE_ARGB RECTFMT_ARGB

// The native m68k ABI places tag/value varargs in consecutive 32-bit slots.
// OS4's va_getlinearva and VARARGS68K bridge are not part of this ABI.
static inline Object* DoSuperNew(struct IClass* cl, Object* object, ...)
{
    va_list arguments;
    va_start(arguments, object);
    struct opSet message;
    message.MethodID = OM_NEW;
    message.ops_AttrList = (struct TagItem*)arguments;
    message.ops_GInfo = NULL;
    Object* result = (Object*)DoSuperMethodA(cl, object, (Msg)&message);
    va_end(arguments);
    return result;
}

static inline APTR AllocVecTaskPooled(ULONG size) { return calloc(1, size); }
static inline void FreeVecTaskPooled(APTR memory) { free(memory); }

// Odyssey uses stccpy only for bounded, NUL-terminated copies and ignores
// its return value. Keep the helper available in strict C++ mode too.
static inline void stccpy(char* destination, const char* source, size_t capacity)
{
    if (!capacity)
        return;
    while (capacity > 1 && *source) {
        *destination++ = *source++;
        --capacity;
    }
    *destination = '\0';
}

#endif
