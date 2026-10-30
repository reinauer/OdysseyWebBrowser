#pragma once

#include <stdint.h>
#include <wtf/Assertions.h>

namespace WTF {

// The m68k ABI only requires word alignment. In particular Exec saves a
// 16-bit SR between the PC and the integer registers on a suspended stack.
template<typename Visitor>
inline void forEachAmigaOS3ConservativePointer(void* begin, void* end, const Visitor& visit)
{
    uintptr_t first = reinterpret_cast<uintptr_t>(begin);
    uintptr_t last = reinterpret_cast<uintptr_t>(end);
    RELEASE_ASSERT(!(first & 1) && !(last & 1) && first <= last);
    for (; last - first >= sizeof(uintptr_t); first += 2) {
        uintptr_t candidate;
        __builtin_memcpy(&candidate, reinterpret_cast<const void*>(first), sizeof(candidate));
        visit(reinterpret_cast<void*>(candidate));
    }
}

}
