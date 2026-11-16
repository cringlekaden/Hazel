#include "Hazel/Core/Log.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Platform/macOS/Metal/MetalQuadFrame.h"
#include "Platform/macOS/Metal/MetalShader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class F> void Reject(const char* name, F&& action)
{
    try { action(); } catch (const std::exception& error) { std::cout << "PASS rejected " << name << ": " << error.what() << '\n'; return; }
    throw std::runtime_error(std::string("Expected rejection: ") + name);
}
Hazel::TextureSpecification Specification(bool linear = false)
{
    Hazel::TextureSpecification specification;
    specification.Width = specification.Height = 2;
    specification.GenerateMips = false;
    specification.MinFilter = specification.MagFilter = linear ? Hazel::TextureFilter::Linear : Hazel::TextureFilter::Nearest;
    specification.WrapS = specification.WrapT = Hazel::TextureWrap::ClampToEdge;
    return specification;
}
glm::mat4 Transform(float x, float z, float width = 1.5f, float height = 3.f)
{
    return glm::translate(glm::mat4(1), {x, 0, z}) * glm::scale(glm::mat4(1), {width, height, 1});
}
void Wait(Hazel::MetalRendererAPI& backend, const std::shared_ptr<const Hazel::MetalQuadReadback>& result)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!result->Completed)
    {
        backend.PollOffscreenQuads();
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Quad completion timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(result->Error.empty(), result->Error);
}
void Pixel(const Hazel::MetalQuadReadback& result, size_t x, size_t y, glm::vec4 color, int32_t id, float depth)
{
    const auto i = y * result.Width + x;
    Check(result.Entity[i] == id, "Signed ID/orientation mismatch at " + std::to_string(x) + "," + std::to_string(y));
    for (int c = 0; c < 4; ++c) Check(std::abs(result.Color[i][c] - color[c]) <= 1e-6f, "Color/UV/sampling/version mismatch at " + std::to_string(x) + "," + std::to_string(y) + " channel=" + std::to_string(c) + " actual=" + std::to_string(result.Color[i][c]) + " expected=" + std::to_string(color[c]));
    Check(std::abs(result.Depth[i] - depth) <= 1e-6f, "Depth/camera/version mismatch");
}
void Save(const Hazel::MetalQuadReadback& result, const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary);
    for (size_t i = 0; i < result.Color.size(); ++i)
    {
        output.write(reinterpret_cast<const char*>(&result.Color[i]), 16);
        output.write(reinterpret_cast<const char*>(&result.Entity[i]), 4);
        output.write(reinterpret_cast<const char*>(&result.Depth[i]), 4);
    }
    Check(bool(output), "Cannot save quad readback");
}
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Usage: MetalRenderer2DQuadSmoke resource-root evidence-directory");
        const auto output = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        std::filesystem::create_directories(output);
        auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
        Hazel::Log::Init();
        const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        Hazel::Resources::Configure({root, root, output / "user-data"});
        Hazel::RenderCommand::Configure(Hazel::RendererAPI::API::Metal);
        auto& backend = static_cast<Hazel::MetalRendererAPI&>(Hazel::RenderCommand::Backend());
        auto observer = backend.Observe();
        Reject("full Renderer2D initialization", [] { Hazel::Renderer2D::Init(); });
        Reject("unsupported default mip texture", [] { Hazel::Texture2D::Create(2, 2); });
        Reject("unsupported attachment extent", [&] { backend.BeginOffscreenQuadFrame(33, 32); });
        Reject("scene without target", [] { Hazel::Renderer2D::BeginScene(glm::mat4(1)); });
        auto texture = Hazel::Texture2D::Create(Specification());
        const std::array<uint32_t, 4> imageA{0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff};
        const std::array<uint32_t, 4> imageB{0xffffff00, 0xffff00ff, 0xff00ffff, 0xff000000};
        const std::array<glm::vec4, 4> colorsA{{{1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,1,1}}};
        const std::array<glm::vec4, 4> colorsB{{{0,1,1,1}, {1,0,1,1}, {1,1,0,1}, {0,0,0,1}}};
        auto transparent = imageA; transparent[0] = 0x000000ff;
        Reject("unsupported translucent upload", [&] { texture->SetData(transparent.data(), sizeof(transparent)); });
        texture->SetData(imageA.data(), sizeof(imageA));
        auto native = std::dynamic_pointer_cast<Hazel::MetalTexture2D>(texture);
        std::weak_ptr<const Hazel::MetalTextureVersion> oldA = native->Snapshot();
        const auto projection = glm::ortho(-2.f, 2.f, -2.f, 2.f, -1.f, 1.f);
        const auto cameraA = projection * glm::translate(glm::mat4(1), {-.5f, 0, 0});
        const auto cameraB = projection * glm::translate(glm::mat4(1), {.5f, 0, 0});
        const glm::vec4 tintA{2,.5f,.25f,1}, tintB{.5f,1,2,1}, clear{.125f,.25f,.5f,1};
        backend.BeginOffscreenQuadFrame(32, 32, clear);
        Hazel::Renderer2D::BeginScene(cameraA);
        Reject("unsupported translucent tint", [&] { Hazel::Renderer2D::DrawQuad(Transform(-.5f, .5f), texture, 1, glm::vec4(.5f), -1); });
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, .5f), texture, 1, tintA, -123456789);
        Hazel::Renderer2D::EndScene();
        texture->SetData(imageB.data(), sizeof(imageB));
        std::weak_ptr<const Hazel::MetalTextureVersion> oldB = native->Snapshot();
        Hazel::Renderer2D::BeginScene(cameraB);
        Hazel::Renderer2D::DrawQuad(Transform(.5f, .5f), texture, 1, tintB, 123456789);
        Hazel::Renderer2D::EndScene();
        Hazel::Renderer2D::BeginScene(cameraA);
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, -.5f), texture, 1, glm::vec4(1), 42); // Farther, rejected by depth.
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 2.f), texture, 1, glm::vec4(1), 43); // Before near plane, clipped.
        Hazel::Renderer2D::EndScene();
        texture->SetData(imageA.data(), sizeof(imageA)); // Must not replace either captured version.
        Check(!oldA.expired() && !oldB.expired(), "Captured texture versions retired before submission");
        auto numeric = backend.SubmitOffscreenQuadFrame();
        Wait(backend, numeric);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 4 && y < 28 && ((x >= 2 && x < 14) || (x >= 18 && x < 30)))
            {
                const bool left = x < 14;
                const auto local = x - (left ? 2 : 18);
                const size_t col = local < 6 ? 0 : 1, row = y < 16 ? 1 : 0;
                Pixel(*numeric, x, y, (left ? colorsA : colorsB)[row * 2 + col] * (left ? tintA : tintB), left ? -123456789 : 123456789, .25f);
            }
            else Pixel(*numeric, x, y, clear, -1, 1);
        }
        Check(oldA.expired() && oldB.expired(), "Completed draw versions were not retired");
        Save(*numeric, output / "nearest-camera-versions.bin");
        std::cout << "PASS nearest/camera/version/depth: 1024 pixels, two cameras and texture versions, exact signed IDs, HDR, far/near rejection\n";

        // Nonconstant 2x2 texture with linear filtering: CPU bilinear oracle at pixel centers.
        auto linearSpec = Specification(true); linearSpec.Format = Hazel::ImageFormat::RGBA32F;
        auto linear = Hazel::Texture2D::Create(linearSpec); linear->SetData(colorsA.data(), sizeof(colorsA));
        backend.BeginOffscreenQuadFrame(32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), linear, 1, glm::vec4(1), -987654321);
        Hazel::Renderer2D::EndScene(); auto filtered = backend.SubmitOffscreenQuadFrame(); Wait(backend, filtered);
        Save(*filtered, output / "linear.bin");
        for (size_t y = 8; y < 24; ++y) for (size_t x = 8; x < 24; ++x)
        {
            const float u = (float(x) + .5f - 8) / 16, v = 1 - (float(y) + .5f - 8) / 16;
            const float a = std::clamp(u * 2 - .5f, 0.f, 1.f), b = std::clamp(v * 2 - .5f, 0.f, 1.f);
            const auto expected = glm::mix(glm::mix(colorsA[0], colorsA[1], a), glm::mix(colorsA[2], colorsA[3], a), b);
            Pixel(*filtered, x, y, expected, -987654321, .5f);
        }
        Save(*filtered, output / "linear.bin");
        std::cout << "PASS linear sampling: 256 pixels, clamp bilinear oracle\n";
        auto repeatSpec = Specification(); repeatSpec.WrapS = repeatSpec.WrapT = Hazel::TextureWrap::Repeat;
        auto repeat = Hazel::Texture2D::Create(repeatSpec); repeat->SetData(imageA.data(), sizeof(imageA));
        backend.BeginOffscreenQuadFrame(32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), repeat, 2, glm::vec4(1), -135791113);
        Hazel::Renderer2D::EndScene(); auto tiled = backend.SubmitOffscreenQuadFrame(); Wait(backend, tiled);
        for (size_t y = 8; y < 24; ++y) for (size_t x = 8; x < 24; ++x)
        {
            const float u = (float(x) + .5f - 8) / 8, v = 2 - (float(y) + .5f - 8) / 8;
            const size_t col = (u - std::floor(u)) < .5f ? 0 : 1, row = (v - std::floor(v)) < .5f ? 0 : 1;
            Pixel(*tiled, x, y, colorsA[row * 2 + col], -135791113, .5f);
        }
        Save(*tiled, output / "repeat.bin");
        std::cout << "PASS repeat sampling: 256 pixels, tiling factor two and repeat addressing\n";
        for (int frame = 0; frame < 12; ++frame)
        {
            backend.BeginOffscreenQuadFrame(32, 32);
            Hazel::Renderer2D::BeginScene(glm::mat4(1));
            Hazel::Renderer2D::DrawQuad(Transform(0, 0, frame % 2 ? -1.f : 1.f, 1), texture, 1, glm::vec4(1), 1000 + frame);
            Hazel::Renderer2D::EndScene(); auto result = backend.SubmitOffscreenQuadFrame(); Wait(backend, result);
            Pixel(*result, 12, 12, colorsA[frame % 2 ? 3 : 2], 1000 + frame, .5f);
            Pixel(*result, 0, 0, glm::vec4(0), -1, 1);
        }
        Save(*numeric, output / "nearest-after-reuse.bin");
        auto release = backend.GateQueueForTesting();
        std::array<std::shared_ptr<const Hazel::MetalQuadReadback>, 3> pending;
        std::array<std::weak_ptr<const Hazel::MetalTextureVersion>, 3> versions;
        for (size_t i = 0; i < pending.size(); ++i)
        {
            auto transient = Hazel::Texture2D::Create(Specification()); transient->SetData(imageA.data(), sizeof(imageA));
            versions[i] = std::dynamic_pointer_cast<Hazel::MetalTexture2D>(transient)->Snapshot();
            backend.BeginOffscreenQuadFrame(32, 32);
            Hazel::Renderer2D::BeginScene(glm::mat4(1));
            Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), transient, 1, glm::vec4(1), int(2000 + i));
            Hazel::Renderer2D::EndScene(); pending[i] = backend.SubmitOffscreenQuadFrame();
            transient.reset(); Check(!versions[i].expired() && !pending[i]->Completed && pending[i]->Color.empty(), "Pending resource/result lifetime mismatch");
        }
        const auto busy = observer->Snapshot();
        Check(busy.InFlight == 3, "Failed to exercise all three M1 slots");
        backend.BeginOffscreenQuadFrame(32, 32);
        Check(!backend.SubmitOffscreenQuadFrame(), "Busy allocator slot was reused");
        backend.CancelOffscreenQuadFrame();
        auto shader = backend.LoadShader("Renderer2D_Quad"); std::weak_ptr<Hazel::Shader> shaderLifetime = shader; shader.reset();
        std::thread unblock([release] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); release(); });
        Hazel::RenderCommand::Shutdown(); unblock.join();
        for (size_t i = 0; i < pending.size(); ++i)
        {
            Check(pending[i]->Completed && pending[i]->Error.empty() && versions[i].expired(), "Shutdown did not retire pending resources");
            Pixel(*pending[i], 12, 12, colorsA[2], int(2000 + i), .5f);
        }
        Check(shaderLifetime.expired(), "Shader outlived completed backend cache shutdown");
        const auto final = observer->Snapshot();
        Check(final.Errors == 0 && final.InFlight == 0 && final.Submitted == 18 && final.Completed == 18 && final.PendingAtShutdown == 3 &&
            final.SlotReuses >= 15 && final.ResourceBytes == 0 && final.PeakResourceBytes <= 1024 * 1024 && final.PeakAllocatorBytes <= 16 * 1024 * 1024,
            "Frame retirement/reuse/accounting failed");
        std::ofstream metrics(output / "metrics.json");
        metrics << "{\"submitted\":" << final.Submitted << ",\"completed\":" << final.Completed << ",\"slotReuses\":" << final.SlotReuses
            << ",\"peakInFlight\":" << final.PeakInFlight << ",\"pendingShutdown\":" << final.PendingAtShutdown << ",\"frameBytes\":" << final.ResourceBytes
            << ",\"peakFrameBytes\":" << final.PeakResourceBytes << ",\"peakAllocatorBytes\":" << final.PeakAllocatorBytes << ",\"errors\":" << final.Errors << "}\n";
        std::cout << "PASS: production Renderer2D quad seam, 18 completed frames, 12 successive submissions, three pending shutdown, busy-slot rejection and exact CPU readback\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
