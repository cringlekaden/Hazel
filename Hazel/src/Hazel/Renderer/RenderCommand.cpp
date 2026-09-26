#include "hzpch.h"

#include "Hazel/Renderer/RenderCommand.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Hazel {

    RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;

}