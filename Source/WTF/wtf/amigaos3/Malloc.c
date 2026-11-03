/* The AmigaOS 3 heap: Doug Lea's malloc 2.8.6 (dlmalloc.c) over Exec memory.
 *
 * The SDK's MemMap allocator adds a 16-byte header to every block, makes a
 * separate AllocMem call for each request of 32 KB or more, copies on every
 * realloc and has no memalign, so each 16 KB GC block cost 32 KB. dlmalloc
 * carves 256 KB segments obtained from Exec, serves larger requests with
 * their own Exec block, reallocates in place where it can, aligns without
 * doubling the request and returns empty segments to Exec.
 *
 * Link with --wrap for newlib's four reentrant entry points, so calls from
 * inside libc arrive here as well. One Exec semaphore serializes the heap; it
 * is set up on first use, before any C++ initialization.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <stabs.h>
#include "Malloc.h"

/* Every Exec block behind the heap carries this header. The list lets the
 * exit code return all of them. The header also keeps two blocks that Exec
 * placed next to each other from looking contiguous to dlmalloc, which would
 * merge them into one segment that could not be returned whole. */
struct SystemBlock {
    struct MinNode node;
    ULONG size; /* bytes handed to dlmalloc */
    ULONG check; /* address ^ SYSTEM_BLOCK_MAGIC while allocated */
};
#define SYSTEM_BLOCK_MAGIC 0x4f574248UL

static struct MinList systemBlocks = {
    (struct MinNode*)&systemBlocks.mlh_Tail, NULL, (struct MinNode*)&systemBlocks.mlh_Head
};
static int heapReleased;

static void* systemMap(size_t size)
{
    struct SystemBlock* block = AllocMem(sizeof(*block) + size, MEMF_ANY);
    if (!block)
        return (void*)~(size_t)0; /* dlmalloc's MFAIL */
    block->size = size;
    block->check = (ULONG)block ^ SYSTEM_BLOCK_MAGIC;
    AddTail((struct List*)&systemBlocks, (struct Node*)&block->node);
    return block + 1;
}

static int systemUnmap(void* address, size_t size)
{
    /* While trimming, dlmalloc may ask to unmap only the tail of a segment.
     * Exec memory goes back whole, so refuse anything but a whole block. */
    struct SystemBlock* block = (struct SystemBlock*)address - 1;
    if (block->check != ((ULONG)block ^ SYSTEM_BLOCK_MAGIC) || block->size != size)
        return -1;
    block->check = 0;
    Remove((struct Node*)&block->node);
    FreeMem(block, sizeof(*block) + size);
    return 0;
}

#define DLMALLOC_EXPORT static __attribute__((unused))
#define USE_DL_PREFIX 1
#define USE_LOCKS 0 /* the semaphore below serializes every call */
#define HAVE_MORECORE 0
#define HAVE_MMAP 1
#define HAVE_MREMAP 0
#define MMAP_CLEARS 0 /* Exec memory is not cleared; calloc must clear it */
#define MMAP(size) systemMap(size)
#define DIRECT_MMAP(size) systemMap(size)
#define MUNMAP(address, size) systemUnmap((address), (size))
#define LACKS_SYS_MMAN_H 1
#define LACKS_TIME_H 1 /* time() would enter gettimeofday() and getenv() */
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1
#define malloc_getpagesize ((size_t)4096U)
#define DEFAULT_GRANULARITY ((size_t)256U * 1024U)
#define DEFAULT_MMAP_THRESHOLD ((size_t)256U * 1024U)
#define DEFAULT_TRIM_THRESHOLD ((size_t)1024U * 1024U)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "dlmalloc.c"
#pragma GCC diagnostic pop

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
    /* Exec semaphores recurse for their owning task. Acquiring this lock
     * never allocates. */
    ObtainSemaphore(&heapSemaphore);
}

static void unlockHeap(void)
{
    ReleaseSemaphore(&heapSemaphore);
}

struct _reent;
void* lockedMalloc(struct _reent*, size_t) __asm__("___wrap__malloc_r");
void lockedFree(struct _reent*, void*) __asm__("___wrap__free_r");
void* lockedCalloc(struct _reent*, size_t, size_t) __asm__("___wrap__calloc_r");
void* lockedRealloc(struct _reent*, void*, size_t) __asm__("___wrap__realloc_r");

void* lockedMalloc(struct _reent* context, size_t size)
{
    (void)context;
    lockHeap();
    void* result = heapReleased ? NULL : dlmalloc(size);
    unlockHeap();
    return result;
}

void lockedFree(struct _reent* context, void* memory)
{
    (void)context;
    lockHeap();
    if (!heapReleased)
        dlfree(memory);
    unlockHeap();
}

void* lockedCalloc(struct _reent* context, size_t count, size_t size)
{
    (void)context;
    lockHeap();
    void* result = heapReleased ? NULL : dlcalloc(count, size);
    unlockHeap();
    return result;
}

void* lockedRealloc(struct _reent* context, void* memory, size_t size)
{
    (void)context;
    lockHeap();
    void* result = heapReleased ? NULL : dlrealloc(memory, size);
    unlockHeap();
    return result;
}

void* amigaos3AlignedAlloc(size_t alignment, size_t size)
{
    lockHeap();
    void* result = heapReleased ? NULL : dlmemalign(alignment, size);
    unlockHeap();
    return result;
}

size_t amigaos3HeapFootprint(void)
{
    lockHeap();
    size_t result = dlmalloc_footprint();
    unlockHeap();
    return result;
}

void amigaos3HeapTrim(void)
{
    lockHeap();
    if (!heapReleased)
        dlmalloc_trim(0);
    unlockHeap();
}

/* Low-memory reserve. Exec calls memory handlers under Forbid, in the task
 * whose AllocMem failed, possibly while that task is inside dlmalloc; the
 * handler therefore touches only the reserve, never the heap. */
#define RESERVE_SIZE (1024UL * 1024UL)
static struct Interrupt lowMemoryInterrupt;
static struct Task* volatile lowMemoryTask;
static ULONG lowMemoryMask;
static BYTE lowMemorySignal = -1;
static ULONG reserveFlags;
static void* volatile reserve;
/* A second block that only a fatal error frees: by then the reserve above
 * is usually spent, and the requester and the shutdown need memory too. */
static void* volatile fatalReserve;

static LONG __attribute__((used)) lowMemoryHandler(struct MemHandlerData* data __asm__("a0"),
                                                   APTR userData __asm__("a1"))
{
    (void)data;
    (void)userData;
    struct Task* task = lowMemoryTask;
    if (!task)
        return MEM_DID_NOTHING;
    Signal(task, lowMemoryMask);
    void* block = reserve;
    if (!block)
        return MEM_DID_NOTHING;
    reserve = NULL;
    FreeMem(block, RESERVE_SIZE);
    return MEM_TRY_AGAIN;
}

void amigaos3ReleaseReserve(void)
{
    Forbid();
    void* block = reserve;
    void* fatalBlock = fatalReserve;
    reserve = NULL;
    fatalReserve = NULL;
    Permit();
    if (block)
        FreeMem(block, RESERVE_SIZE);
    if (fatalBlock)
        FreeMem(fatalBlock, RESERVE_SIZE);
}

unsigned long amigaos3WatchLowMemory(void)
{
    if (lowMemoryTask)
        return lowMemoryMask;
    if (SysBase->LibNode.lib_Version < 39)
        return 0; /* AddMemHandler() */
    lowMemorySignal = AllocSignal(-1);
    if (lowMemorySignal < 0)
        return 0;
    lowMemoryMask = 1UL << lowMemorySignal;
    /* Hold the reserve in fast memory; chip memory belongs to the display. */
    reserveFlags = AvailMem(MEMF_FAST) ? MEMF_FAST : MEMF_ANY;
    reserve = AllocMem(RESERVE_SIZE, reserveFlags);
    fatalReserve = AllocMem(RESERVE_SIZE, reserveFlags);
    lowMemoryInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    lowMemoryInterrupt.is_Node.ln_Pri = 0;
    lowMemoryInterrupt.is_Node.ln_Name = (char*)"OWB heap reserve";
    lowMemoryInterrupt.is_Data = NULL;
    lowMemoryInterrupt.is_Code = (void (*)(void))lowMemoryHandler;
    lowMemoryTask = FindTask(NULL);
    AddMemHandler(&lowMemoryInterrupt);
    return lowMemoryMask;
}

int amigaos3LowMemoryHandled(void)
{
    if (!lowMemoryTask)
        return 0;
    SetSignal(0, lowMemoryMask);
    if (!reserve) {
        void* block = AllocMem(RESERVE_SIZE, reserveFlags);
        Forbid();
        if (!reserve) {
            reserve = block;
            block = NULL;
        }
        Permit();
        if (block)
            FreeMem(block, RESERVE_SIZE);
    }
    return reserve != NULL;
}

void amigaos3StopWatchingLowMemory(void)
{
    if (!lowMemoryTask)
        return;
    RemMemHandler(&lowMemoryInterrupt);
    lowMemoryTask = NULL;
    amigaos3ReleaseReserve();
    FreeSignal(lowMemorySignal);
    lowMemorySignal = -1;
}

/* Runs after C++ destructors and stdio shutdown, at the SDK heap's own
 * priority, and before libraries are closed. */
static void __attribute__((used)) releaseHeap(void)
{
    amigaos3StopWatchingLowMemory();
    lockHeap();
    heapReleased = 1;
    struct SystemBlock* block;
    while ((block = (struct SystemBlock*)RemHead((struct List*)&systemBlocks)))
        FreeMem(block, sizeof(*block) + block->size);
    unlockHeap();
}
ADD2EXIT(releaseHeap, -99);
