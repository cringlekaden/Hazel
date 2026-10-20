#include "GPUTimer.h"
#include "Platform/OpenGL/OpenGLGPUTimer.h"
#include "RendererAPI.h"
#include "hzpch.h"
namespace Hazel
{
Scope<GPUTimer> GPUTimer::Create()
{
    switch (RendererAPI::GetAPI())
    {
    case RendererAPI::API::OpenGL:
        return CreateScope<OpenGLGPUTimer>();
    default:
        return {};
    }
}
} // namespace Hazel
