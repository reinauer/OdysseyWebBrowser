/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OSAllocator.h"

#include <wtf/PageBlock.h>

namespace WTF {

// Classic Exec has no virtual memory reservation or page protection. Back
// reservations with aligned physical memory and retain it until release.
void* OSAllocator::reserveUncommitted(size_t bytes, Usage, bool, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    // Neither executable JIT memory nor protected guard pages are supported.
    if (executable || jitCageEnabled || includesGuardPages)
        CRASH();
    void* result = fastAlignedMalloc(pageSize(), bytes);
    memset(result, 0, bytes);
    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    return reserveUncommitted(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
}

void OSAllocator::commit(void* address, size_t bytes, bool, bool executable)
{
    if (executable)
        CRASH();
    memset(address, 0, bytes);
}

void OSAllocator::decommit(void*, size_t)
{
    // Retain the reservation; commit zeroes any reused range.
}

void OSAllocator::releaseDecommitted(void* address, size_t)
{
    fastAlignedFree(address);
}

void OSAllocator::hintMemoryNotNeededSoon(void*, size_t)
{
    // Exec cannot discard physical pages from a live allocation.
}

} // namespace WTF
