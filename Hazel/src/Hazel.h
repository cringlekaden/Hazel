#pragma once

// For use by Hazel applications
#include "Hazel/Core/Base.h"
#include "Hazel/Core/Assert.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Debug/Instrumentor.h"

#include "Hazel/Core/Application.h"
#include "Hazel/Core/Layer.h"
#include "Hazel/Core/Timestep.h"
#include "Hazel/ImGui/ImGuiLayer.h"

#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/MouseButtonCodes.h"
#include "Hazel/Core/MouseCodes.h"

#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Resources/Buffer.h"
#include "Hazel/Renderer/Cameras/Camera.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include "Hazel/Scene/SceneCamera.h"
#include "Hazel/Renderer/Text/Font.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/ScriptableEntity.h"
#include "Hazel/Renderer/Cameras/NavigationCamera.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
#include "Hazel/Renderer/Resources/Shader.h"
#include "Hazel/Renderer/Resources/Texture.h"
#include "Hazel/Renderer/Resources/VertexArray.h"
#include "Hazel/Renderer/Cameras/OrthographicCamera.h"
#include "Hazel/Renderer/Cameras/PlanarCameraController.h"

#include "Hazel/Project/Project.h"

// Runtime game UI configuration/model (independent of editor ImGui).
#include "Hazel/UI/RuntimeUIModel.h"
