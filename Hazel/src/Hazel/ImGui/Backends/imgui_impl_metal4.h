// dear imgui: Renderer Backend for Metal 4
// This needs to be used along with a Platform Backend (e.g. OSX)
// Metal 4 requires Apple Silicon and macOS 26+.

// Implemented features:
//  [X] Renderer: User texture binding. Use a full-width borrowed MTLTexture pointer as texture identifier; Hazel retains its immutable owner. Read the FAQ about ImTextureID/ImTextureRef!
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Multi-viewport support (multiple windows).
// Missing features or Issues:
//  [ ] Texture view pool support? Reevaluate which type to use for ImtextureID.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// ObjC API
//-----------------------------------------------------------------------------

#ifdef __OBJC__

@class MTL4RenderPassDescriptor;
@protocol MTLTexture, MTLSamplerState, MTLDevice, MTL4CommandBuffer, MTL4RenderCommandEncoder, MTL4CommandQueue;

// framesInFlight must match the number of frames your application keeps in flight (e.g. the size of your own
// command buffer/allocator ring). The backend uses it to size its own per-frame-in-flight resources (constant
// buffer, vertex/index buffer cache) so the CPU never overwrites a slot the GPU may still be reading.
// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool ImGui_ImplMetal4_Init(id<MTLDevice> device, id<MTL4CommandQueue> commandQueue, int framesInFlight);
IMGUI_IMPL_API void ImGui_ImplMetal4_Shutdown();
IMGUI_IMPL_API void ImGui_ImplMetal4_SetTextureSampler(id<MTLTexture> texture, id<MTLSamplerState> sampler);
// frameInFlightIndex must match the slot you use to index your own per-frame-in-flight resources
// (e.g. the same index used to pick your command buffer/allocator), and must be < framesInFlight passed to Init().
// Before calling NewFrame(), wait for the GPU to finish any work your application previously submitted for that slot.
IMGUI_IMPL_API void ImGui_ImplMetal4_NewFrame(MTL4RenderPassDescriptor* renderPassDescriptor, int frameInFlightIndex);
IMGUI_IMPL_API void ImGui_ImplMetal4_RenderDrawData(ImDrawData* draw_data,
                                                   id<MTL4CommandBuffer> commandBuffer,
                                                   id<MTL4RenderCommandEncoder> commandEncoder);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool ImGui_ImplMetal4_CreateDeviceObjects(id<MTLDevice> device);
IMGUI_IMPL_API void ImGui_ImplMetal4_DestroyDeviceObjects();

#endif

//-----------------------------------------------------------------------------
// Shared
//-----------------------------------------------------------------------------
#include <cstdint>
IMGUI_IMPL_API uint64_t ImGui_ImplMetal4_ResourceBytes();

#endif // IMGUI_DISABLE
