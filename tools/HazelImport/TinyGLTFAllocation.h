#pragma once
#include "Allocation.h"
#define TINYGLTF3_MALLOC(size) hz_import_allocate(size)
#define TINYGLTF3_REALLOC(pointer,size) hz_import_reallocate(pointer,size)
#define TINYGLTF3_FREE(pointer) hz_import_free(pointer)
