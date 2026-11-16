#pragma once
#include "Hazel/Renderer/Drawing/Renderer2DQuad.h"
#include "MetalTexture.h"
#include <Metal/Metal.hpp>
#include <memory>
#include <atomic>
#include <vector>
#include <string>

namespace Hazel {
class MetalShader;
struct MetalQuadDraw
{
    std::vector<QuadVertex> Vertices;
    uint32_t First = 0, Count = 0;
    glm::mat4 Camera;
    std::array<std::shared_ptr<const MetalTextureVersion>, 32> Textures;
};
struct MetalQuadReadback
{
    uint64_t Serial = 0;
    uint32_t Width = 0, Height = 0;
    bool Completed = false;
    std::string Error;
    // Populated only by completion polling, after ordered GPU completion.
    std::vector<glm::vec4> Color;
    std::vector<int32_t> Entity;
    std::vector<float> Depth;
};
struct MetalQuadFrame
{
    uint32_t Width = 0, Height = 0;
    glm::vec4 Clear{0};
    std::shared_ptr<MetalShader> Shader;
    std::vector<MetalQuadDraw> Draws;
    std::shared_ptr<MetalQuadReadback> Result;
    std::vector<glm::vec4> CompletedColor;
    std::vector<int32_t> CompletedEntity;
    std::vector<float> CompletedDepth;
    std::shared_ptr<std::string> FeedbackError = std::make_shared<std::string>();
    std::shared_ptr<std::atomic<bool>> Feedback = std::make_shared<std::atomic<bool>>(false);
    std::array<NS::SharedPtr<MTL::Texture>, 3> Targets;
    std::vector<NS::SharedPtr<MTL::Buffer>> Buffers;
    std::vector<NS::SharedPtr<MTL4::ArgumentTable>> Arguments;
    NS::SharedPtr<MTL::ResidencySet> Residency;
    NS::SharedPtr<MTL::DepthStencilState> DepthState;
    void Complete();
    uint64_t ResourceBytes() const;
};
}
