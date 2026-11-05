#pragma once
#include <stdlib.h>
#include "Allocation.h"
#define malloc(size) hz_import_allocate(size)
#define free(pointer) hz_import_free(pointer)
