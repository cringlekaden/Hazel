#pragma once
#include "Hazel/Renderer/Resources/VertexArray.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
#include <Metal/Metal.hpp>
#include <memory>
#include <vector>

namespace Hazel {
class MetalDevice;
class MetalVertexBuffer final : public VertexBuffer
{
public:
    MetalVertexBuffer(uint32_t size, const void* data = nullptr);
    void Bind() const override {}
    void Unbind() const override {}
    void SetData(const void* data, uint32_t size) override;
    const BufferLayout& GetLayout() const override { return m_Layout; }
    void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
    const NS::SharedPtr<MTL::Buffer>& Snapshot() const { return m_Buffer; }
private:
    uint32_t m_Size;
    BufferLayout m_Layout;
    NS::SharedPtr<MTL::Buffer> m_Buffer;
};
class MetalIndexBuffer final : public IndexBuffer
{
public:
    MetalIndexBuffer(const uint32_t* indices, uint32_t count);
    void Bind() const override {}
    void Unbind() const override {}
    uint32_t GetCount() const override { return m_Count; }
    const NS::SharedPtr<MTL::Buffer>& Snapshot() const { return m_Buffer; }
private:
    uint32_t m_Count;
    NS::SharedPtr<MTL::Buffer> m_Buffer;
};
class MetalVertexArray final : public VertexArray
{
public:
    void Bind() const override {}
    void Unbind() const override {}
    void AddVertexBuffer(const Ref<VertexBuffer>& buffer) override;
    void SetIndexBuffer(const Ref<IndexBuffer>& buffer) override;
    const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_Vertices; }
    const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_Indices; }
private:
    std::vector<Ref<VertexBuffer>> m_Vertices;
    Ref<IndexBuffer> m_Indices;
};
class MetalUniformBuffer final : public UniformBuffer
{
public:
    MetalUniformBuffer(uint32_t size, uint32_t binding);
    void Bind() const override;
    void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
private:
    uint32_t m_Binding;
    std::shared_ptr<std::vector<uint8_t>> m_Data;
};
}
