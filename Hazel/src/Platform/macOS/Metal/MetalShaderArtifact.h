#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hazel {
struct MetalShaderStageData
{
    std::vector<uint32_t> Spirv;
    std::string Library;
    // Producer diagnostics only; never required or stored in the runtime package.
    std::string Source, MSL, Reflection;
};
struct MetalShaderArtifact
{
    std::string Name, SourceIdentity, Producer;
    uint32_t TextureSlots = 32;
    std::array<MetalShaderStageData, 2> Stages; // vertex, fragment

    static std::string Profile();
    static std::string Digest(const std::string& bytes);
    static std::string SourceKey(const std::string& name, const std::string& source, uint32_t slots);
    static void ValidateName(const std::string& name, uint32_t slots);
    static MetalShaderArtifact Decode(const std::string& bytes);
    std::string Encode() const;
    void Validate(bool libraries = true) const;
};
// Offline producer and qualification share this single M2-A translation path.
MetalShaderArtifact CompileMetalShader(const std::string& name, const std::string& source, uint32_t slots);
std::string ReflectMetalShader(const std::vector<uint32_t>& words);
std::string ReadMetalShaderFile(const std::filesystem::path& path);
void WriteMetalShaderFile(const std::filesystem::path& path, const std::string& bytes);
}
