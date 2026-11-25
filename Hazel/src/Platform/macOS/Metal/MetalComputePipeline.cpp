#include "MetalComputePipeline.h"
#include "MetalDevice.h"
#include "Hazel/Core/FileSystem.h"
#include <dispatch/dispatch.h>
#include <stdexcept>
namespace Hazel {
MetalComputePipeline::MetalComputePipeline(std::shared_ptr<MetalDevice> device, const std::filesystem::path& library, const std::string& entry) : m_Device(std::move(device)) {
    if (!m_Device || entry.empty()) throw std::invalid_argument("Compute pipeline requires a device and entry point");
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    ScopedBuffer contents(FileSystem::ReadFileBinary(library));
    if (!contents || contents.Size() > 16 * 1024 * 1024) throw std::invalid_argument("Compute metallib is missing or exceeds 16 MiB");
    auto bytes = dispatch_data_create(contents.Data(), contents.Size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error* error = nullptr;
    m_Library = NS::TransferPtr(m_Device->NativeDevice()->newLibrary(bytes, &error));
    dispatch_release(bytes);
    if (!m_Library) throw std::runtime_error(error ? error->localizedDescription()->utf8String() : "Cannot load compute metallib");
    bool found = false;
    const auto* names = m_Library->functionNames();
    for (size_t i = 0; i < names->count(); ++i) if (entry == names->object<NS::String>(i)->utf8String()) found = true;
    if (!found) throw std::invalid_argument("Compute entry point is missing: " + entry);
    auto compilerDescriptor = NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
    auto compiler = NS::TransferPtr(m_Device->NativeDevice()->newCompiler(compilerDescriptor.get(), &error));
    if (!compiler) throw std::runtime_error("Cannot create native Metal 4 compute compiler");
    auto function = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    function->setLibrary(m_Library.get()); function->setName(NS::String::string(entry.c_str(), NS::UTF8StringEncoding));
    auto descriptor = NS::TransferPtr(MTL4::ComputePipelineDescriptor::alloc()->init());
    auto options = NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
    options->setShaderReflection(MTL4::ShaderReflectionBindingInfo | MTL4::ShaderReflectionBufferTypeInfo);
    descriptor->setOptions(options.get());
    descriptor->setComputeFunctionDescriptor(function.get());
    m_Pipeline = NS::TransferPtr(compiler->newComputePipelineState(descriptor.get(), nullptr, &error));
    if (!m_Pipeline) throw std::runtime_error(error ? error->localizedDescription()->utf8String() : "Cannot compile native Metal 4 compute pipeline");
    const auto* reflection = m_Pipeline->reflection();
    if (!reflection || !reflection->bindings()) throw std::runtime_error("Compute reflection is unavailable");
    if (entry=="cullLocalLights") {
        unsigned mask=0;
        for (size_t i=0;i<reflection->bindings()->count();++i) {
            auto* b=reflection->bindings()->object<MTL::Binding>(i);
            if (!b->isUsed()) continue;
            if(b->type()==MTL::BindingTypeBuffer && b->index()<3 && b->access()==MTL::BindingAccessReadOnly) {
                const size_t sizes[]={15376,128,32};
                if(static_cast<MTL::BufferBinding*>(b)->bufferDataSize()!=sizes[b->index()])
                    throw std::runtime_error("Local culling buffer ABI differs");
                mask|=1u<<b->index();
            } else if(b->type()==MTL::BindingTypeTexture && b->index()==0) {
                auto* t=static_cast<MTL::TextureBinding*>(b);
                if(t->textureType()!=MTL::TextureType2D || t->textureDataType()!=MTL::DataTypeUInt || t->access()!=MTL::BindingAccessWriteOnly)
                    throw std::runtime_error("Local culling tile texture ABI differs");
                mask|=8;
            } else throw std::runtime_error("Unexpected local culling resource");
        }
        if(mask!=15)throw std::runtime_error("Incomplete local culling reflection");
        return;
    }
    unsigned textures = 0, buffers = 0;
    for (size_t i = 0; i < reflection->bindings()->count(); ++i) {
        const auto* binding = reflection->bindings()->object<MTL::Binding>(i);
        if (!binding->isUsed()) continue;
        if (binding->type() == MTL::BindingTypeBuffer && binding->index() == 0 && binding->access() == MTL::BindingAccessReadOnly) {
            m_ParameterBytes = static_cast<const MTL::BufferBinding*>(binding)->bufferDataSize(); ++buffers;
        } else if (binding->type() == MTL::BindingTypeTexture) {
            const auto* texture = static_cast<const MTL::TextureBinding*>(binding);
            if (texture->index() > 1 || texture->textureType() != MTL::TextureType2D || texture->textureDataType() != MTL::DataTypeFloat || texture->arrayLength() != 1 ||
                texture->access() != (texture->index() == 0 ? MTL::BindingAccessReadOnly : MTL::BindingAccessWriteOnly))
                throw std::invalid_argument("Compute texture binding differs from the color-pass ABI");
            ++textures;
        } else throw std::invalid_argument("Compute resource binding differs from the color-pass ABI");
    }
    if (textures != 2 || buffers != 1 || !m_ParameterBytes || m_ParameterBytes > 256) throw std::invalid_argument("Compute reflection differs from the color-pass ABI");
    if (m_Pipeline->maxTotalThreadsPerThreadgroup() < 64) throw std::runtime_error("Compute pipeline cannot execute the 8x8 color-pass contract");
}
}
