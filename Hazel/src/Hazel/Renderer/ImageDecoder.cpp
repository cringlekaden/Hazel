#include "hzpch.h"
#include "ImageDecodeMemory.h"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace Hazel
{
namespace
{
union Block {
    size_t Bytes;
    std::max_align_t Alignment;
};
thread_local size_t Limit = SIZE_MAX, Live = 0;
thread_local bool Exhausted = false;
} // namespace
void *ImageDecodeAllocate(size_t bytes)
{
    if (!bytes)
        bytes = 1;
    if (bytes > SIZE_MAX - sizeof(Block) || bytes + sizeof(Block) > Limit ||
        Live > Limit - bytes - sizeof(Block))
    {
        Exhausted = true;
        return nullptr;
    }
    auto block = static_cast<Block *>(std::malloc(bytes + sizeof(Block)));
    if (!block)
    {
        Exhausted = true;
        return nullptr;
    }
    block->Bytes = bytes;
    Live += bytes + sizeof(Block);
    return block + 1;
}
void ImageDecodeFree(void *pointer)
{
    if (pointer)
    {
        auto block = static_cast<Block *>(pointer) - 1;
        Live -= block->Bytes + sizeof(Block);
        std::free(block);
    }
}
void *ImageDecodeReallocate(void *pointer, size_t bytes)
{
    if (!pointer)
        return ImageDecodeAllocate(bytes);
    if (!bytes)
    {
        ImageDecodeFree(pointer);
        return nullptr;
    }
    auto size = (static_cast<Block *>(pointer) - 1)->Bytes;
    auto next = ImageDecodeAllocate(bytes);
    if (next)
    {
        std::memcpy(next, pointer, std::min(size, bytes));
        ImageDecodeFree(pointer);
    }
    return next;
}
ImageDecodeBudget::ImageDecodeBudget(size_t bytes) : m_Previous(Limit)
{
    if (Live)
        throw std::runtime_error("Image decoder has an outstanding allocation on this thread");
    Limit = bytes ? bytes : SIZE_MAX;
    Exhausted = false;
}
ImageDecodeBudget::~ImageDecodeBudget()
{
    Limit = m_Previous;
}
bool ImageDecodeBudget::Failed() const
{
    return Exhausted;
}
} // namespace Hazel
#define STBI_MALLOC(size) Hazel::ImageDecodeAllocate(size)
#define STBI_REALLOC(pointer, size) Hazel::ImageDecodeReallocate(pointer, size)
#define STBI_FREE(pointer) Hazel::ImageDecodeFree(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
