#include "hzpch.h"
#include "ComputePipeline.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Renderer.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Platform/macOS/Metal/MetalComputePipeline.h"
#endif
#include <stdexcept>
namespace Hazel {
Ref<ComputePipeline> ComputePipeline::Create(const std::filesystem::path& library, const std::string& entry) {
#ifdef HZ_PLATFORM_MACOS
    if (Renderer::GetAPI() == RendererAPI::API::Metal)
        return CreateRef<MetalComputePipeline>(static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device(), library, entry);
#else
    (void)library; (void)entry;
#endif
    throw std::logic_error("Native compute passes are unavailable on this graphics backend");
}
}
