/* The AmigaOS 3 heap in Malloc.c: Doug Lea's malloc over Exec memory. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory aligned to a power of two; release it with free(). */
void* amigaos3AlignedAlloc(size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif
