#pragma once
#include <iostream>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

// Read-only in-process evidence works inside a process sandbox, where dyld
// environment diagnostics may be restricted. Never installed into a product.
inline void PrintLoadedNativeImages() {
#if defined(__APPLE__)
    const auto count=_dyld_image_count();
    for(uint32_t index=0;index<count;++index)
        if(const auto* name=_dyld_get_image_name(index))std::cout<<"LOADED_IMAGE "<<name<<'\n';
#endif
}
