#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* One producer operation per thread; all upstream C allocations use this cap. */
void hz_import_memory_begin(size_t limit);
size_t hz_import_memory_peak(void);
int hz_import_memory_failed(void);
size_t hz_import_memory_live(void);
void *hz_import_allocate(size_t size);
void *hz_import_reallocate(void *pointer, size_t size);
void hz_import_free(void *pointer);
#ifdef __cplusplus
}
#endif
