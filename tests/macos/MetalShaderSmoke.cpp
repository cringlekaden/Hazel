#include "Hazel/Core/Log.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Resources/Shader.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Platform/macOS/Metal/MetalShader.h"
#include <functional>
#include <iostream>
#include <thread>

namespace {
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
void Reject(const std::string& label, const std::function<void()>& action, const std::string& diagnostic)
{
    try { action(); }
    catch (const std::exception& error)
    {
        Check(std::string(error.what()).find(diagnostic) != std::string::npos, label + " wrong diagnostic: " + error.what());
        std::cout << "PASS rejected " << label << ": " << error.what() << '\n';
        return;
    }
    throw std::runtime_error("Expected rejection: " + label);
}
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Usage: MetalShaderSmoke resources user-data");
        auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
        Hazel::Log::Init();
        const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        const auto user = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        Hazel::Resources::Configure({root, root, user});
        Hazel::RenderCommand::Configure(Hazel::RendererAPI::API::Metal);
        auto& backend = static_cast<Hazel::MetalRendererAPI&>(Hazel::RenderCommand::Backend());
        const auto linePath = root / "shaders/metal/Renderer2D_Line.hzmetal";
        const auto bytes = Hazel::ReadMetalShaderFile(linePath);
        const auto original = Hazel::MetalShaderArtifact::Decode(bytes);
        Check(original.Encode() == bytes, "Canonical artifact round-trip differs");
        Reject("truncated header", [&] { Hazel::MetalShaderArtifact::Decode(bytes.substr(0, 40)); }, "magic/version/size");
        Reject("truncated payload", [&] { Hazel::MetalShaderArtifact::Decode(bytes.substr(0, bytes.size() - 1)); }, "checksum");
        Reject("trailing payload", [&] { Hazel::MetalShaderArtifact::Decode(bytes + "x"); }, "checksum");
        auto stale = bytes;
        stale[80] ^= 1; // First profile bytes, followed by a deliberately repaired checksum.
        stale.replace(8, 64, Hazel::MetalShaderArtifact::Digest(stale.substr(72)));
        Reject("stale profile with valid checksum", [&] { Hazel::MetalShaderArtifact::Decode(stale); }, "profile");
        auto wrongStage = original;
        std::swap(wrongStage.Stages[0], wrongStage.Stages[1]);
        Reject("wrong stage", [&] { wrongStage.Encode(); }, "stage/entry-point");
        auto wrongSpirv = original;
        wrongSpirv.Stages[0].Spirv[0] = 0;
        Reject("SPIR-V corruption", [&] { wrongSpirv.Encode(); }, "SPIR-V");
        Reject("invalid variant", [&] { backend.LoadShader("Renderer2D_Line", 16); }, "variant");
        Reject("unknown shader", [&] { backend.LoadShader("../escape"); }, "shader/variant");
        Reject("noncanonical shader reference", [&] { Hazel::Shader::Create((root.parent_path() / "unrelated/Renderer2D_Line.glsl").u8string()); }, "canonical");
        Reject("inline GLSL", [&] { Hazel::Shader::Create("inline", "vertex", "fragment"); }, "offline");

        // Failed loads must not publish an in-memory entry or damage other owners.
        Hazel::WriteMetalShaderFile(linePath, "corrupt");
        Reject("bad packaged artifact", [&] { backend.LoadShader("Renderer2D_Line"); }, "magic/version/size");
        Hazel::WriteMetalShaderFile(linePath, bytes);
        auto badLibrary = original;
        badLibrary.Stages[0].Library = "not a metallib";
        Hazel::WriteMetalShaderFile(linePath, badLibrary.Encode());
        Reject("invalid native library", [&] { backend.LoadShader("Renderer2D_Line"); }, "metallib load");
        auto mismatch = original;
        const auto quad = Hazel::MetalShaderArtifact::Decode(Hazel::ReadMetalShaderFile(root / "shaders/metal/Renderer2D_Quad-32.hzmetal"));
        mismatch.Stages[0].Library = quad.Stages[0].Library;
        Hazel::WriteMetalShaderFile(linePath, mismatch.Encode());
        Reject("native/SPIR-V layout mismatch", [&] { backend.LoadShader("Renderer2D_Line"); }, "Metal shader");
        Hazel::WriteMetalShaderFile(linePath, bytes);
        auto line = Hazel::Shader::Create((root / "shaders/Renderer2D_Line.glsl").u8string());
        Check(line->GetProgramLoadingPath() == Hazel::Shader::ProgramLoadingPath::MetalLibrary, "Factory did not load Metal library");
        Reject("draw binding unavailable", [&] { line->Bind(); }, "drawing");
        Reject("loose uniform unavailable", [&] { line->SetInt("u_ID", 1); }, "uniform");
        const auto cache = user / "cache/shader/metal4" / (Hazel::MetalShaderArtifact::Digest(bytes) + ".hzmetal");
        for (const auto& corrupt : {std::string("truncated"), quad.Encode()})
        {
            Hazel::WriteMetalShaderFile(cache, corrupt);
            auto repaired = std::make_shared<Hazel::MetalShader>(backend.Device(), "Renderer2D_Line");
            Check(Hazel::ReadMetalShaderFile(cache) == bytes && repaired->Pipeline(), "Cache recovery failed");
        }
        std::filesystem::remove(cache);
        auto cold = std::make_shared<Hazel::MetalShader>(backend.Device(), "Renderer2D_Line");
        Check(Hazel::ReadMetalShaderFile(cache) == bytes, "Cold cache not seeded");
        const auto source = root / "shaders/Renderer2D_Line.glsl";
        Hazel::WriteMetalShaderFile(source, "Changed canonical GLSL");
        Reject("stale source identity", [&] { Hazel::MetalShader shader(backend.Device(), "Renderer2D_Line"); }, "stale");
        std::filesystem::remove(source);
        std::shared_ptr<Hazel::Shader> concurrent[2];
        std::exception_ptr errors[2];
        std::thread first([&] { try { concurrent[0] = backend.LoadShader("Renderer2D_Quad"); } catch (...) { errors[0] = std::current_exception(); } });
        std::thread second([&] { try { concurrent[1] = backend.LoadShader("Renderer2D_Quad"); } catch (...) { errors[1] = std::current_exception(); } });
        first.join(); second.join();
        for (auto& error : errors) if (error) std::rethrow_exception(error);
        Check(concurrent[0] == concurrent[1] && concurrent[0], "Concurrent loads published duplicate resources");
        for (const auto& name : {"DisplayOutput", "EntityOutline", "EnvironmentSky", "ForwardPBR", "Renderer2D_Circle", "Renderer2D_Text", "WorldPresentation"})
            Check(bool(Hazel::Shader::Create((root / (std::string("shaders/") + name + ".glsl")).u8string())), "Production corpus loading failed");
        Check(bool(backend.LoadShader("Renderer2D_Quad", 16)), "16-slot variant unavailable");
        std::weak_ptr<Hazel::Shader> retained = line;
        line.reset(); cold.reset();
        Check(!retained.expired(), "Backend did not retain immutable shader resource");
        concurrent[0].reset(); concurrent[1].reset();
        Hazel::RenderCommand::Shutdown();
        Check(retained.expired(), "Backend shader cache was not released after drain");
        Reject("post-shutdown load", [&] { backend.LoadShader("Renderer2D_Line"); }, "shutdown");
        std::cout << "PASS: production factory, all variants, concurrent ownership, cache cold/hit/corrupt recovery, source/profile/stage validation, native mismatch diagnostics, source-free loading and drain-before-release\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
