#include "Allocation.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#if defined(_MSC_VER)
#define HZ_IMPORT_THREAD __declspec(thread)
#else
#define HZ_IMPORT_THREAD _Thread_local
#endif
typedef union hz_import_block { size_t size; max_align_t alignment; } hz_import_block;
static HZ_IMPORT_THREAD size_t limit_bytes, live_bytes, peak_bytes;
static HZ_IMPORT_THREAD int allocation_failed;
void hz_import_memory_begin(size_t limit) {
    /* A caller must release the previous arena before beginning another parse. */
    if(live_bytes){allocation_failed=1;return;}
    limit_bytes=limit;peak_bytes=0;allocation_failed=0;
}
size_t hz_import_memory_peak(void){return peak_bytes;}
size_t hz_import_memory_live(void){return live_bytes;}
int hz_import_memory_failed(void){return allocation_failed;}
void *hz_import_allocate(size_t size) {
    hz_import_block *block;
    if(!size)size=1;
    if(size>SIZE_MAX-sizeof(hz_import_block) || size+sizeof(hz_import_block)>limit_bytes || live_bytes>limit_bytes-size-sizeof(hz_import_block)) {
        allocation_failed=1;return NULL;
    }
    block=(hz_import_block*)malloc(sizeof(hz_import_block)+size);
    if(!block){allocation_failed=1;return NULL;}
    block->size=size;live_bytes+=size+sizeof(hz_import_block);
    if(live_bytes>peak_bytes)peak_bytes=live_bytes;
    return block+1;
}
void hz_import_free(void *pointer) {
    if(pointer){hz_import_block *block=((hz_import_block*)pointer)-1;live_bytes-=block->size+sizeof(hz_import_block);free(block);}
}
void *hz_import_reallocate(void *pointer,size_t size) {
    void *next;size_t old;
    if(!pointer)return hz_import_allocate(size);
    if(!size){hz_import_free(pointer);return NULL;}
    old=(((hz_import_block*)pointer)-1)->size;
    /* Count simultaneous old/new storage rather than understate peak memory. */
    next=hz_import_allocate(size);if(!next)return NULL;
    memcpy(next,pointer,old<size?old:size);hz_import_free(pointer);return next;
}
