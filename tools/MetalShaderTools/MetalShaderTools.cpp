#include "Platform/macOS/Metal/MetalShaderArtifact.h"
#include <filesystem>
#include <iostream>
#include <cstring>

int main(int argc, char** argv)
{
    try
    {
        if (argc != 6) throw std::runtime_error("Usage: MetalShaderTools --translate|--pack|--check name slots source-path build-directory");
        const std::string mode = argv[1], name = argv[2];
        const uint32_t slots = std::string(argv[3]) == "16" ? 16 : std::string(argv[3]) == "32" ? 32 : 0;
        const std::filesystem::path directory = argv[5];
        const auto variant = name + (name == "Renderer2D_Quad" ? "-" + std::to_string(slots) : "");
        const auto source = Hazel::ReadMetalShaderFile(std::filesystem::u8path(argv[4]));
        if (mode == "--check")
        {
            const auto artifact = Hazel::MetalShaderArtifact::Decode(Hazel::ReadMetalShaderFile(directory / (variant + ".hzmetal")));
            if (artifact.SourceIdentity != Hazel::MetalShaderArtifact::SourceKey(name, source, slots) ||
                artifact.Name != name || artifact.TextureSlots != slots || artifact.Producer != Hazel::ReadMetalShaderFile(directory / "producer.txt"))
                throw std::runtime_error("Stale shader artifact identity");
        }
        else if (mode == "--translate")
        {
            const auto artifact = Hazel::CompileMetalShader(name, source, slots);
            Hazel::WriteMetalShaderFile(directory / (variant + ".source-key"), artifact.SourceIdentity);
            for (size_t i = 0; i < 2; ++i)
            {
                const auto stage = variant + (i == 0 ? ".vertex" : ".fragment");
                const auto& data = artifact.Stages[i];
                Hazel::WriteMetalShaderFile(directory / (stage + ".metal"), data.MSL);
                Hazel::WriteMetalShaderFile(directory / (stage + ".spv"), {reinterpret_cast<const char*>(data.Spirv.data()), data.Spirv.size() * 4});
            }
        }
        else if (mode == "--pack")
        {
            Hazel::MetalShaderArtifact artifact;
            artifact.Name = name; artifact.TextureSlots = slots;
            artifact.SourceIdentity = Hazel::MetalShaderArtifact::SourceKey(name, source, slots);
            artifact.Producer = Hazel::ReadMetalShaderFile(directory / "producer.txt");
            if (Hazel::ReadMetalShaderFile(directory / (variant + ".source-key")) != artifact.SourceIdentity)
                throw std::runtime_error("Source changed after translation; regenerate stages");
            for (size_t i = 0; i < 2; ++i)
            {
                const auto stage = variant + (i == 0 ? ".vertex" : ".fragment");
                const auto words = Hazel::ReadMetalShaderFile(directory / (stage + ".spv"));
                if (words.size() % 4) throw std::runtime_error("Unaligned SPIR-V");
                auto& data = artifact.Stages[i];
                data.Spirv.resize(words.size() / 4);
                std::memcpy(data.Spirv.data(), words.data(), words.size());
                data.Library = Hazel::ReadMetalShaderFile(directory / (stage + ".metallib"));
            }
            Hazel::WriteMetalShaderFile(directory / (variant + ".hzmetal"), artifact.Encode());
        }
        else throw std::runtime_error("Unknown operation");
        std::cout << "PASS " << mode << ' ' << variant << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL MetalShaderTools: " << error.what() << '\n'; return 1; }
}
