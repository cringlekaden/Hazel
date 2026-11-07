#pragma once
#include <cstddef>
namespace Hazel
{
// Shared stb implementation hooks. Ordinary callers remain unscoped; bounded
// import decoding limits all decoder allocations, including PNG inflate buffers.
void *ImageDecodeAllocate(size_t bytes);
void *ImageDecodeReallocate(void *pointer, size_t bytes);
void ImageDecodeFree(void *pointer);
class ImageDecodeBudget
{
  public:
    explicit ImageDecodeBudget(size_t bytes);
    ~ImageDecodeBudget();
    bool Failed() const;
    ImageDecodeBudget(const ImageDecodeBudget &) = delete;
    ImageDecodeBudget &operator=(const ImageDecodeBudget &) = delete;

  private:
    size_t m_Previous;
};
} // namespace Hazel
