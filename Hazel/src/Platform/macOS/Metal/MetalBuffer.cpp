#include "hzpch.h"
#include "MetalBuffer.h"
#include "MetalRendererAPI.h"
#include "MetalDevice.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <cstring>
#include <stdexcept>

namespace Hazel {
namespace {
MetalRendererAPI& Backend()
{
    auto* backend = dynamic_cast<MetalRendererAPI*>(&RenderCommand::Backend());
    if (!backend) throw std::logic_error("Metal buffer binding requires an active Metal backend");
    return *backend;
}
NS::SharedPtr<MTL::Buffer> Allocate(size_t size, const void* data)
{
    if (!size || size > 128ULL * 1024 * 1024) throw std::invalid_argument("Metal buffer exceeds 128 MiB");
    auto buffer = NS::TransferPtr(Backend().Device()->NativeDevice()->newBuffer(size, MTL::ResourceStorageModeShared));
    if (!buffer) throw std::runtime_error("Cannot allocate Metal mesh buffer");
    if (data) std::memcpy(buffer->contents(), data, size);
    else std::memset(buffer->contents(), 0, size);
    return buffer;
}
}
MetalVertexBuffer::MetalVertexBuffer(uint32_t size, const void* data) : m_Size(size), m_Buffer(Allocate(size, data)) {}
void MetalVertexBuffer::SetData(const void* data, uint32_t size)
{
    if (!data || size != m_Size) throw std::invalid_argument("Metal vertex upload requires complete buffer");
    m_Buffer = Allocate(size, data); // Submitted draws retain earlier native versions.
}
MetalIndexBuffer::MetalIndexBuffer(const uint32_t* indices, uint32_t count)
    : m_Count(count), m_Buffer(Allocate(size_t(count) * 4, indices))
{
    if (!indices) throw std::invalid_argument("Metal indices require data");
}
void MetalVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& buffer)
{
    if (!std::dynamic_pointer_cast<MetalVertexBuffer>(buffer) || !m_Vertices.empty())
        throw std::invalid_argument("Metal mesh requires one interleaved native vertex stream");
    m_Vertices.push_back(buffer);
}
void MetalVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& buffer)
{
    if (!std::dynamic_pointer_cast<MetalIndexBuffer>(buffer)) throw std::invalid_argument("Metal mesh requires native indices");
    m_Indices = buffer;
}
MetalUniformBuffer::MetalUniformBuffer(uint32_t size, uint32_t binding) : m_Binding(binding)
{
    if (!size || size > 16384 || binding >= 8) throw std::invalid_argument("Metal uniform binding/size out of bounds");
    m_Data = std::make_shared<std::vector<uint8_t>>(size);
    Bind();
}
void MetalUniformBuffer::Bind() const { Backend().BindUniform(m_Binding, m_Data); }
void MetalUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
{
    if (!data || offset > m_Data->size() || size > m_Data->size() - offset) throw std::invalid_argument("Metal uniform upload out of bounds");
    std::memcpy(m_Data->data() + offset, data, size);
}
}
