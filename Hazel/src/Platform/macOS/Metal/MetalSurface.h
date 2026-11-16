#pragma once
#include <cstdint>
struct GLFWwindow;
namespace CA { class MetalLayer; }
namespace MTL { class Device; }
namespace Hazel {
struct MetalSurfaceSize { uint32_t Width=0, Height=0, LogicalWidth=0, LogicalHeight=0; double Scale=1; bool Visible=false; };
// Cocoa bridge: attach returns a +1 layer, consumed by C++ RAII.
CA::MetalLayer* AttachMetalLayer(GLFWwindow* window, MTL::Device* device);
void DetachMetalLayer(GLFWwindow* window, CA::MetalLayer* layer);
MetalSurfaceSize UpdateMetalLayer(GLFWwindow* window, CA::MetalLayer* layer);
}
