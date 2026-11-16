#include "Hazel/Core/Log.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#include "Platform/macOS/Metal/MetalTexture.h"
#include "Platform/macOS/Metal/MetalShader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
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
Hazel::FramebufferSpecification TargetSpecification(uint32_t width = 32, uint32_t height = 32)
{
    Hazel::FramebufferSpecification specification;
    specification.Width = width; specification.Height = height;
    specification.Attachments = {Hazel::FramebufferTextureFormat::RGBA32F,
        Hazel::FramebufferTextureFormat::RED_INTEGER, Hazel::FramebufferTextureFormat::DEPTH32F};
    return specification;
}
void Begin(const Hazel::Ref<Hazel::Framebuffer>& framebuffer, uint32_t width, uint32_t height, glm::vec4 clear = glm::vec4(0))
{
    if (framebuffer->GetSpecification().Width != width || framebuffer->GetSpecification().Height != height)
        framebuffer->Resize(width, height);
    framebuffer->Bind(); framebuffer->Clear(clear);
}
Hazel::Ref<const Hazel::FramebufferReadback> Submit(const Hazel::Ref<Hazel::Framebuffer>& framebuffer)
{
    framebuffer->Unbind(); return framebuffer->Submit();
}
glm::mat4 Transform(float x, float z, float width = 1.5f, float height = 3.f)
{
    return glm::translate(glm::mat4(1), {x, 0, z}) * glm::scale(glm::mat4(1), {width, height, 1});
}
void Wait(Hazel::Framebuffer& framebuffer, const std::shared_ptr<const Hazel::FramebufferReadback>& result)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!result->Completed)
    {
        framebuffer.Poll();
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Quad completion timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(result->Error.empty(), result->Error);
}
void Pixel(const Hazel::FramebufferReadback& result, size_t x, size_t y, glm::vec4 color, int32_t id, float depth)
{
    const auto i = y * result.Width + x;
    Check(result.Entity[i] == id, "Signed ID/orientation mismatch at " + std::to_string(x) + "," + std::to_string(y));
    for (int c = 0; c < 4; ++c) Check(std::abs(result.Color[i][c] - color[c]) <= 1e-6f, "Color/UV/sampling/version mismatch at " + std::to_string(x) + "," + std::to_string(y) + " channel=" + std::to_string(c) + " actual=" + std::to_string(result.Color[i][c]) + " expected=" + std::to_string(color[c]));
    Check(std::abs(result.Depth[i] - depth) <= 1e-6f, "Depth/camera/version mismatch");
}
void Save(const Hazel::FramebufferReadback& result, const std::filesystem::path& path)
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
        auto framebuffer = Hazel::Framebuffer::Create(TargetSpecification());
        Check(bool(std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(framebuffer)), "Factory did not return a production Metal framebuffer");
        Reject("full Renderer2D initialization", [] { Hazel::Renderer2D::Init(); });
        Hazel::Renderer2D::InitQuads();
        Reject("duplicate quad initialization", [] { Hazel::Renderer2D::InitQuads(); });
        Reject("unsupported default mip texture", [] { Hazel::Texture2D::Create(2, 2); });
        Reject("unsupported attachment extent", [&] { Hazel::Framebuffer::Create(TargetSpecification(33, 32)); });
        auto badProfile = TargetSpecification(); badProfile.Samples = 2;
        Reject("multisample framebuffer", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.SwapChainTarget = true;
        Reject("swapchain framebuffer", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.Attachments.Attachments[2].TextureFormat = Hazel::FramebufferTextureFormat::DEPTH24STENCIL8;
        Reject("unqualified depth format", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.Attachments.Attachments[0].TextureFormat = Hazel::FramebufferTextureFormat::RGBA16F;
        Reject("unqualified color format", [&] { Hazel::Framebuffer::Create(badProfile); });
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
        Begin(framebuffer, 32, 32, clear);
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
        auto numeric = Submit(framebuffer);
        Wait(*framebuffer, numeric);
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
        Check(framebuffer->ReadPixel(1, 3, 5) == -123456789 && framebuffer->ReadDepthPixel(3, 5) == .25f &&
            framebuffer->ReadColorPixel(0, 0, 0) == clear, "Factory virtual readback mismatch");
        Reject("integer attachment as color", [&] { framebuffer->ReadColorPixel(1, 0, 0); });
        Reject("negative readback coordinate", [&] { framebuffer->ReadPixel(1, -1, 0); });
        Reject("out-of-range readback coordinate", [&] { framebuffer->ReadDepthPixel(32, 0); });
        Reject("GL renderer ID", [&] { framebuffer->GetColorAttachmentRendererID(); });
        Reject("output encoding", [&] { framebuffer->EncodeOutput(); });
        Reject("compositing", [&] { framebuffer->CompositeWorldTo({}, 1); });
        Reject("outline", [&] { framebuffer->DrawEntityOutlineTo({}, 1, 1, glm::vec4(1)); });
        Check(oldA.expired() && oldB.expired(), "Completed draw versions were not retired");
        Save(*numeric, output / "nearest-camera-versions.bin");
        std::cout << "PASS nearest/camera/version/depth: 1024 pixels, two cameras and texture versions, exact signed IDs, HDR, far/near rejection\n";

        // Empty flush/end must not manufacture draws. Empty submission proves attachment clears.
        Hazel::Renderer2D::ResetStats();
        Hazel::Renderer2D::Flush();
        Begin(framebuffer, 32, 32, clear);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::Flush(); Hazel::Renderer2D::EndScene();
        auto empty = Submit(framebuffer); Wait(*framebuffer, empty);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x) Pixel(*empty, x, y, clear, -1, 1);
        Check(Hazel::Renderer2D::GetStats().DrawCalls == 0 && Hazel::Renderer2D::GetStats().UploadedBytes == 0,
            "Empty flush changed draw/upload statistics");

        auto second = Hazel::Texture2D::Create(Specification()); second->SetData(imageB.data(), sizeof(imageB));
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginOrderedScene(Hazel::Camera{}, glm::mat4(1));
        Reject("nested scene", [] { Hazel::Renderer2D::BeginScene(glm::mat4(1)); });
        Reject("circle", [] { Hazel::Renderer2D::DrawCircle(glm::mat4(1), glm::vec4(1)); });
        Reject("line", [] { Hazel::Renderer2D::DrawLine({}, {}, glm::vec4(1)); });
        Reject("clip", [] { Hazel::Renderer2D::SetPrimitiveClip({0,0,1,1}); });
        auto invalid = glm::mat4(1); invalid[0][0] = std::numeric_limits<float>::quiet_NaN();
        Hazel::Renderer2D::DrawQuad(invalid, texture);
        Reject("invalid chunk flush", [] { Hazel::Renderer2D::Flush(); });
        Hazel::Renderer2D::ResetStats();
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 0, 1, 1), texture, 1, glm::vec4(1), -2000000001);
        texture->SetData(imageB.data(), sizeof(imageB));
        Hazel::Renderer2D::DrawQuad(Transform(.5f, 0, 1, 1), texture, 1, glm::vec4(1), 2000000001);
        texture->SetData(imageA.data(), sizeof(imageA)); // both pending versions must survive until flush/completion
        // Equal-depth overlap: first submitted quad wins with LESS. Texture slot one is reused.
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 0, 1, 1), texture, 1, glm::vec4(.5f,.5f,.5f,1), 99);
        Hazel::Renderer2D::Flush(); Hazel::Renderer2D::Flush();
        auto stats = Hazel::Renderer2D::GetStats();
        Check(stats.QuadCount == 3 && stats.GeometryCount == 3 && stats.DrawCalls == 1 && stats.UploadedBytes == 768 &&
            stats.Flushes[size_t(Hazel::Renderer2D::FlushReason::External)] == 1,
            "Shared quad run did not coalesce, consume once, or account correctly");
        // A second run in the same scene must keep the earlier immutable vertex/camera/descriptor data.
        second->SetData(imageA.data(), sizeof(imageA)); // earlier right-hand run must still sample B
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, .5f, 1, 1), second, 1, glm::vec4(1), 98);
        Hazel::Renderer2D::EndScene();
        stats = Hazel::Renderer2D::GetStats();
        Check(stats.QuadCount == 4 && stats.DrawCalls == 2 && stats.UploadedBytes == 1024 &&
            stats.Flushes[size_t(Hazel::Renderer2D::FlushReason::End)] == 1, "Second shared flush statistics mismatch");
        auto ordered = Submit(framebuffer); Wait(*framebuffer, ordered);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 8 && y < 24)
            {
                const bool left = x < 16;
                const size_t col = (x % 16) < 8 ? 0 : 1, row = y < 16 ? 1 : 0;
                Pixel(*ordered, x, y, (left ? colorsA : colorsB)[row * 2 + col], left ? -2000000001 : 2000000001, .5f);
            }
            else Pixel(*ordered, x, y, glm::vec4(0), -1, 1);
        }
        Save(*ordered, output / "ordered-run.bin");
        std::cout << "PASS shared ordered runs: four quads, two draws/flushes, 1024 vertex bytes, two textures/slots, equal-depth ordering; empty clears and rejected chunk recovery\n";
        // CPU shutdown discards unflushed work; reinitialization needs no font service.
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(glm::mat4(1), texture);
        Hazel::Renderer2D::Shutdown(); framebuffer->Cancel();
        Reject("draw after shutdown", [&] { Hazel::Renderer2D::DrawQuad(glm::mat4(1), texture); });
        Hazel::Renderer2D::InitQuads();

        // Full bounded chunk: automatic capacity flush uses all 32 vertices / 48 indices.
        Hazel::Renderer2D::ResetStats();
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        for (int q = 0; q < 8; ++q)
            Hazel::Renderer2D::DrawQuad(Transform(-.875f + q * .25f, 0, .25f, 1), texture, 1, glm::vec4(1), q % 2 ? -100-q : 100+q);
        Hazel::Renderer2D::DrawQuad(glm::mat4(1), texture); // drains the full chunk, then admits one excess quad
        Reject("ninth quad at flush", [] { Hazel::Renderer2D::EndScene(); });
        Hazel::Renderer2D::EndScene(); // rejected chunk was discarded; prior eight remain valid
        const auto capacityStats = Hazel::Renderer2D::GetStats();
        Check(capacityStats.DrawCalls == 1 && capacityStats.QuadCount == 9 && capacityStats.UploadedBytes == 2048 &&
            capacityStats.Flushes[size_t(Hazel::Renderer2D::FlushReason::Capacity)] == 1, "Capacity flush/recovery statistics mismatch");
        auto capacity = Submit(framebuffer); Wait(*framebuffer, capacity);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 8 && y < 24)
            {
                const int q = int(x / 4);
                Pixel(*capacity, x, y, colorsA[(y < 16 ? 2 : 0) + (x % 4 >= 2 ? 1 : 0)], q % 2 ? -100-q : 100+q, .5f);
            }
            else Pixel(*capacity, x, y, glm::vec4(0), -1, 1);
        }
        Save(*capacity, output / "capacity-run.bin");
        std::cout << "PASS full capacity run: eight quads, one draw, 2048 vertex bytes; ninth-quad rejection preserves prior run\n";

        // Nonconstant 2x2 texture with linear filtering: CPU bilinear oracle at pixel centers.
        auto linearSpec = Specification(true); linearSpec.Format = Hazel::ImageFormat::RGBA32F;
        auto linear = Hazel::Texture2D::Create(linearSpec); linear->SetData(colorsA.data(), sizeof(colorsA));
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), linear, 1, glm::vec4(1), -987654321);
        Hazel::Renderer2D::EndScene(); auto filtered = Submit(framebuffer); Wait(*framebuffer, filtered);
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
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), repeat, 2, glm::vec4(1), -135791113);
        Hazel::Renderer2D::EndScene(); auto tiled = Submit(framebuffer); Wait(*framebuffer, tiled);
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
            Begin(framebuffer, 32, 32);
            Hazel::Renderer2D::BeginScene(glm::mat4(1));
            Hazel::Renderer2D::DrawQuad(Transform(0, 0, frame % 2 ? -1.f : 1.f, 1), texture, 1, glm::vec4(1), 1000 + frame);
            Hazel::Renderer2D::EndScene(); auto result = Submit(framebuffer); Wait(*framebuffer, result);
            Pixel(*result, 12, 12, colorsA[frame % 2 ? 3 : 2], 1000 + frame, .5f);
            Pixel(*result, 0, 0, glm::vec4(0), -1, 1);
        }
        Save(*numeric, output / "nearest-after-reuse.bin");
        auto lifecycleGate = backend.GateQueueForTesting();
        std::array<std::weak_ptr<const Hazel::MetalTextureVersion>, 3> lifecycleVersions;
        lifecycleVersions[0] = native->Snapshot();
        auto resized = Hazel::Framebuffer::Create(TargetSpecification(32, 16));
        auto metalResized = std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(resized);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationA = metalResized->Snapshot();
        Begin(resized, 32, 16, glm::vec4(.25f,.5f,2,1));
        resized->ClearAttachment(1, -2000000003);
        Reject("ambient offscreen clear", [] { Hazel::RenderCommand::Clear(); });
        Reject("ambient offscreen clear color", [] { Hazel::RenderCommand::SetClearColor(glm::vec4(1)); });
        Reject("ambient offscreen viewport", [] { Hazel::RenderCommand::SetViewport(0, 0, 1, 1); });
        Reject("resize during captured pass", [&] { resized->Resize(16, 32); });
        Reject("another framebuffer bind", [&] { framebuffer->Bind(); });
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Reject("clear during active scene", [&] { resized->Clear(glm::vec4(1)); });
        Reject("unbind during active scene", [&] { resized->Unbind(); });
        Reject("cancel during active scene", [&] { resized->Cancel(); });
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), texture, 1, glm::vec4(1), -2147483647);
        Hazel::Renderer2D::EndScene();
        Reject("clear after captured draws", [&] { resized->Clear(glm::vec4(1)); });
        resized->Unbind();
        framebuffer->Bind();
        Reject("prior generation read after bind", [&] { framebuffer->ReadPixel(1, 0, 0); });
        framebuffer->Unbind(); // two resource-owned CPU packets can coexist
        auto resizedA = resized->Submit(); framebuffer->Cancel();
        Reject("pending framebuffer read", [&] { resized->ReadPixel(1, 0, 0); });
        resized->Resize(16, 32); // gate remains closed: no global wait can be used here
        Check(!generationA.expired(), "Resize released a submitted attachment generation");
        Begin(resized, 16, 32, glm::vec4(2,.5f,.25f,1));
        resized->ClearAttachment(1, 2000000003);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationB = metalResized->Snapshot();
        texture->SetData(imageB.data(), sizeof(imageB));
        lifecycleVersions[1] = native->Snapshot();
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), texture, 1, glm::vec4(1), 2147483647);
        Hazel::Renderer2D::EndScene(); auto resizedB = Submit(resized);
        // A submitted generation is replaced on Bind; cancel this new unsubmitted pass.
        resized->Bind(); resized->Cancel();
        resized->Resize(8, 8); resized->Cancel();
        Reject("invalid resize", [&] { resized->Resize(0, 8); });
        Check(resized->GetSpecification().Width == 8 && resized->GetSpecification().Height == 8,
            "Failed resize changed the published specification");
        metalResized.reset(); resized.reset(); // destroy framebuffer with both old generations pending
        auto destroyed = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        auto metalDestroyed = std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(destroyed);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationC = metalDestroyed->Snapshot();
        Begin(destroyed, 8, 8, glm::vec4(.125f,.25f,.5f,1));
        destroyed->ClearAttachment(1, std::numeric_limits<int32_t>::min());
        auto transient = Hazel::Texture2D::Create(Specification()); transient->SetData(imageA.data(), sizeof(imageA));
        lifecycleVersions[2] = std::dynamic_pointer_cast<Hazel::MetalTexture2D>(transient)->Snapshot();
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), transient, 1, glm::vec4(1), 2002);
        Hazel::Renderer2D::EndScene();
        auto destroyedResult = Submit(destroyed); transient.reset();
        metalDestroyed.reset(); destroyed.reset();
        Check(!generationA.expired() && !generationB.expired() && !generationC.expired(),
            "Framebuffer replacement/destruction released pending attachments");
        auto cancelled = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> cancelledGeneration =
            std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(cancelled)->Snapshot();
        cancelled->Bind(); cancelled->Unbind();
        Check(!cancelled->Submit(), "Busy allocator unexpectedly reused by framebuffer");
        cancelled->Cancel(); cancelled.reset();
        Check(cancelledGeneration.expired(), "Cancelled unsubmitted attachments leaked");
        auto retry = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        Begin(retry, 8, 8, glm::vec4(.5f,.25f,.125f,1)); retry->ClearAttachment(1, -1900000001);
        retry->Unbind();
        Check(!retry->Submit(), "Expected a retained busy retry packet");
        Check(!resizedA->Completed && !resizedB->Completed && !destroyedResult->Completed,
            "Queue gate did not cover framebuffer lifecycle mutations");
        texture->SetData(imageA.data(), sizeof(imageA)); // release the mutable owner's B version
        for (const auto& version : lifecycleVersions) Check(!version.expired(), "Pending texture version released before completion");
        Check(observer->Snapshot().InFlight == 3, "Failed to gate all three M1 allocator slots");
        lifecycleGate();
        Wait(*framebuffer, resizedA); Wait(*framebuffer, resizedB); Wait(*framebuffer, destroyedResult);
        auto retried = retry->Submit();
        Check(bool(retried) && !retried->Completed, "Busy framebuffer packet could not be retried unchanged");
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> retryGeneration =
            std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(retry)->Snapshot();
        retry.reset();
        Check(!retryGeneration.expired(), "Destroying retried framebuffer released unretired attachments");
        auto shader = backend.LoadShader("Renderer2D_Quad"); std::weak_ptr<Hazel::Shader> shaderLifetime = shader; shader.reset();
        Hazel::Renderer2D::Shutdown();
        Hazel::RenderCommand::Shutdown();
        Check(retried->Completed && retryGeneration.expired(), "Shutdown failed to retire retried framebuffer");
        for (const auto& version : lifecycleVersions) Check(version.expired(), "Shutdown did not retire pending texture versions");
        Check(shaderLifetime.expired(), "Shader outlived completed backend cache shutdown");
        Reject("framebuffer factory after shutdown", [&] { Hazel::Framebuffer::Create(TargetSpecification()); });
        Reject("framebuffer resize after shutdown", [&] { framebuffer->Resize(8, 8); });
        Reject("framebuffer bind after shutdown", [&] { framebuffer->Bind(); });
        for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x)
            Pixel(*retried, x, y, glm::vec4(.5f,.25f,.125f,1), -1900000001, 1);
        Save(*retried, output / "busy-retry.bin");
        for (const auto& result : {resizedA, resizedB})
            for (uint32_t y = 0; y < result->Height; ++y) for (uint32_t x = 0; x < result->Width; ++x)
            {
                const bool first = result == resizedA;
                if (x >= result->Width / 4 && x < result->Width * 3 / 4 &&
                    y >= result->Height / 4 && y < result->Height * 3 / 4)
                    Pixel(*result, x, y, (first ? colorsA : colorsB)[(y < result->Height / 2 ? 2 : 0) + (x >= result->Width / 2 ? 1 : 0)],
                        first ? -2147483647 : 2147483647, .5f);
                else Pixel(*result, x, y, first ? glm::vec4(.25f,.5f,2,1) : glm::vec4(2,.5f,.25f,1),
                    first ? -2000000003 : 2000000003, 1);
            }
        for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x)
        {
            if (x >= 2 && x < 6 && y >= 2 && y < 6)
                Pixel(*destroyedResult, x, y, colorsA[(y < 4 ? 2 : 0) + (x >= 4 ? 1 : 0)], 2002, .5f);
            else Pixel(*destroyedResult, x, y, glm::vec4(.125f,.25f,.5f,1), std::numeric_limits<int32_t>::min(), 1);
        }
        Check(generationA.expired() && generationB.expired() && generationC.expired(),
            "Completed framebuffer generations did not retire");
        Save(*resizedA, output / "resize-a.bin"); Save(*resizedB, output / "resize-b.bin");
        Save(*destroyedResult, output / "destroyed-clear.bin");
        std::cout << "PASS factory framebuffer generations: gated resize/replacement/destruction/cancel, busy retry packet, asymmetric extents and signed clears\n";

        const auto final = observer->Snapshot();
        Check(final.Errors == 0 && final.InFlight == 0 && final.Submitted == 22 && final.Completed == 22 && final.PendingAtShutdown == 1 &&
            final.SlotReuses >= 19 && final.ResourceBytes == 0 && final.PeakResourceBytes <= 1024 * 1024 && final.PeakAllocatorBytes <= 16 * 1024 * 1024,
            "Frame retirement/reuse/accounting failed");
        std::ofstream metrics(output / "metrics.json");
        metrics << "{\"submitted\":" << final.Submitted << ",\"completed\":" << final.Completed << ",\"slotReuses\":" << final.SlotReuses
            << ",\"orderedQuads\":" << stats.QuadCount << ",\"orderedDraws\":" << stats.DrawCalls << ",\"orderedVertexBytes\":" << stats.UploadedBytes
            << ",\"peakInFlight\":" << final.PeakInFlight << ",\"pendingShutdown\":" << final.PendingAtShutdown << ",\"frameBytes\":" << final.ResourceBytes
            << ",\"peakFrameBytes\":" << final.PeakResourceBytes << ",\"peakAllocatorBytes\":" << final.PeakAllocatorBytes << ",\"errors\":" << final.Errors << "}\n";
        std::cout << "PASS: initialized shared Renderer2D quad flush, 22 completed frames, 12 successive submissions, one unretired shutdown and three gated lifecycle submissions, busy-slot rejection and exact CPU readback\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
