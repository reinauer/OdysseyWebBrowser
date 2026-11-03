/* The AmigaOS 3 heap in Malloc.c: Doug Lea's malloc over Exec memory. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory aligned to a power of two; release it with free(). */
void* amigaos3AlignedAlloc(size_t alignment, size_t size);

/* Bytes the heap currently holds from Exec. */
size_t amigaos3HeapFootprint(void);

/* Return empty segments and free space at the top of the heap to Exec. */
void amigaos3HeapTrim(void);

/* Keep a 1 MB reserve and install an Exec memory handler. When an allocation
 * anywhere in the system fails, the handler frees the reserve so that the
 * allocation can be retried, and signals the calling task. Returns that
 * signal mask, or 0 when the handler could not be installed (Exec before
 * V39). Call it once, from the task that will release memory. */
unsigned long amigaos3WatchLowMemory(void);

/* After the watching task has released memory: clear the signal and restore
 * the reserve if memory allows. Returns nonzero when the reserve is held. */
int amigaos3LowMemoryHandled(void);

/* Free the reserve without restoring it; any task may call it. */
void amigaos3ReleaseReserve(void);

/* Remove the handler and free the reserve; call it from the watching task. */
void amigaos3StopWatchingLowMemory(void);

#ifdef __cplusplus
}
#endif
