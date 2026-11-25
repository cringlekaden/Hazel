// Qualification exports the production translator's diagnostics; no second path.
#include "MetalM2A.h"
#include "Platform/macOS/Metal/MetalShaderArtifact.h"
#include <algorithm>
#include <iostream>

namespace MetalM2A {
int Translate(const std::filesystem::path& output)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator("Hazel/Resources/shaders"))
        if (entry.path().extension() == ".glsl") paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    Check(paths.size() == 11, "Raster corpus changed: review qualification coverage");
    for (const auto& path : paths)
        for (const uint32_t slots : path.stem() == "Renderer2D_Quad" ? std::vector<uint32_t>{16, 32} : std::vector<uint32_t>{32})
        {
            const auto name = path.stem().string();
            const auto variant = name + (name == "Renderer2D_Quad" ? "-" + std::to_string(slots) : "");
            const auto artifact = Hazel::CompileMetalShader(name, Read(path), slots);
            Hazel::WriteMetalShaderFile(output / (variant + ".source-key"), artifact.SourceIdentity);
            for (size_t i = 0; i < 2; ++i)
            {
                const auto& data = artifact.Stages[i];
                const auto identity = variant + (i == 0 ? ".vertex" : ".fragment");
                Hazel::WriteMetalShaderFile(output / (identity + ".glsl"), data.Source);
                Hazel::WriteMetalShaderFile(output / (identity + ".spv"), {reinterpret_cast<const char*>(data.Spirv.data()), data.Spirv.size() * 4});
                Hazel::WriteMetalShaderFile(output / (identity + ".metal"), data.MSL);
                Hazel::WriteMetalShaderFile(output / (identity + ".reflection.txt"), data.Reflection);
                std::cout << "PASS translation " << identity << '\n';
            }
        }
    return 0;
}
}
