/* Serialize the SDK's shared MemMap heap before any C++ initialization.
 * This allocator bypasses newlib's __malloc_lock hooks. Link with --wrap
 * for its four reentrant entry points, including calls from inside libc.
 */
#include <stddef.h>
#include <exec/semaphores.h>
#include <proto/exec.h>

struct _reent;
static struct SignalSemaphore heapSemaphore;
static int initialized;

static void lockHeap(void)
{
    Forbid();
    if (!initialized) {
        InitSemaphore(&heapSemaphore);
        initialized = 1;
    }
    Permit();
    /* Exec semaphores recurse for their owning task; calloc/realloc may
       enter malloc/free again. Acquiring this lock never allocates. */
    ObtainSemaphore(&heapSemaphore);
}

extern void* originalMalloc(struct _reent*, size_t) __asm__("___real__malloc_r");
extern void originalFree(struct _reent*, void*) __asm__("___real__free_r");
extern void* originalCalloc(struct _reent*, size_t, size_t) __asm__("___real__calloc_r");
extern void* originalRealloc(struct _reent*, void*, size_t) __asm__("___real__realloc_r");

void* lockedMalloc(struct _reent*, size_t) __asm__("___wrap__malloc_r");
void lockedFree(struct _reent*, void*) __asm__("___wrap__free_r");
void* lockedCalloc(struct _reent*, size_t, size_t) __asm__("___wrap__calloc_r");
void* lockedRealloc(struct _reent*, void*, size_t) __asm__("___wrap__realloc_r");

void* lockedMalloc(struct _reent* context, size_t size)
{
    lockHeap();
    void* result = originalMalloc(context, size);
    ReleaseSemaphore(&heapSemaphore);
    return result;
}

void lockedFree(struct _reent* context, void* memory)
{
    lockHeap();
    originalFree(context, memory);
    ReleaseSemaphore(&heapSemaphore);
}

void* lockedCalloc(struct _reent* context, size_t count, size_t size)
{
    lockHeap();
    void* result = originalCalloc(context, count, size);
    ReleaseSemaphore(&heapSemaphore);
    return result;
}

void* lockedRealloc(struct _reent* context, void* memory, size_t size)
{
    lockHeap();
    void* result = originalRealloc(context, memory, size);
    ReleaseSemaphore(&heapSemaphore);
    return result;
}
