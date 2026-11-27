#include "hzpch.h"
#include "MetalShaderArtifact.h"
#include <CommonCrypto/CommonDigest.h>
#include <hazel_shader_toolchain.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <spirv_cross/spirv_cross.hpp>
#include <unistd.h>

namespace Hazel {
namespace {
constexpr size_t Limit = 64 * 1024 * 1024;
void Check(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("Metal shader artifact: " + message);
}
void Put(std::string& output, const std::string& bytes)
{
    Check(bytes.size() <= Limit, "field exceeds size limit");
    const auto size = static_cast<uint32_t>(bytes.size());
    for (unsigned i = 0; i < 4; ++i) output += static_cast<char>((size >> (8 * i)) & 255);
    output += bytes;
}
std::string Take(const std::string& bytes, size_t& offset)
{
    Check(offset <= bytes.size() && bytes.size() - offset >= 4, "truncated field length");
    uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) size |= uint32_t(static_cast<unsigned char>(bytes[offset++])) << (8 * i);
    Check(size <= Limit && size <= bytes.size() - offset, "truncated/oversized field");
    auto result = bytes.substr(offset, size); offset += size;
    return result;
}
}
std::string MetalShaderArtifact::Profile()
{
    return "hazel-metal4-artifact-v1-vulkan12-opt-debug-msl30-flipy-finaloutput-v2-clipfix-std140-tier2-hzmetal-macos27-" HAZEL_SHADER_PINS_SHA256;
}
std::string MetalShaderArtifact::Digest(const std::string& bytes)
{
    Check(bytes.size() <= Limit, "hash input exceeds size limit");
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), digest);
    std::ostringstream output;
    for (const auto byte : digest) output << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return output.str();
}
void MetalShaderArtifact::ValidateName(const std::string& name, uint32_t slots)
{
    const std::set<std::string> names = {"BloomFilter", "DefocusTile", "DefocusPrefilter", "DefocusGather", "DisplayOutput", "EntityOutline", "EnvironmentSky", "ForwardPBR", "DepthPBR", "MotionPBR", "ScreenReflections",
        "Renderer2D_Circle", "Renderer2D_Line", "Renderer2D_Quad", "Renderer2D_Text", "WorldPresentation"};
    Check(names.count(name) == 1 && (slots == 32 || (name == "Renderer2D_Quad" && slots == 16)), "unsupported shader/variant " + name);
}
std::string MetalShaderArtifact::SourceKey(const std::string& name, const std::string& source, uint32_t slots)
{
    ValidateName(name, slots);
    std::string identity;
    for (const auto& field : {Profile(), name, std::to_string(slots), source}) Put(identity, field);
    return Digest(identity);
}
void MetalShaderArtifact::Validate(bool libraries) const
{
    ValidateName(Name, TextureSlots);
    Check(SourceIdentity.size() == 64 && SourceIdentity.find_first_not_of("0123456789abcdef") == std::string::npos, "invalid source identity");
    if (libraries) Check(!Producer.empty(), "missing Apple producer fingerprint");
    for (size_t i = 0; i < Stages.size(); ++i)
    {
        const auto& stage = Stages[i];
        Check(stage.Spirv.size() >= 5 && stage.Spirv.size() <= Limit / 4 && stage.Spirv.front() == 0x07230203u, "invalid SPIR-V header/size");
        ReflectMetalShader(stage.Spirv); // Includes CPU layout/resource bounds validation.
        spirv_cross::Compiler reflection(stage.Spirv);
        const auto entries = reflection.get_entry_points_and_stages();
        Check(entries.size() == 1 && entries[0].name == "main" &&
            entries[0].execution_model == (i == 0 ? spv::ExecutionModelVertex : spv::ExecutionModelFragment), "SPIR-V stage/entry-point mismatch");
        if (libraries) Check(!stage.Library.empty() && stage.Library.size() <= Limit, "missing/oversized metallib");
    }
}
std::string MetalShaderArtifact::Encode() const
{
    Validate();
    std::string payload;
    for (const auto& field : {Profile(), Name, std::to_string(TextureSlots), SourceIdentity, Producer}) Put(payload, field);
    for (const auto& stage : Stages)
    {
        Put(payload, {reinterpret_cast<const char*>(stage.Spirv.data()), stage.Spirv.size() * 4});
        Put(payload, stage.Library);
    }
    return "HZMETAL1" + Digest(payload) + payload;
}
MetalShaderArtifact MetalShaderArtifact::Decode(const std::string& bytes)
{
    Check(bytes.size() >= 72 && bytes.size() <= Limit && bytes.substr(0, 8) == "HZMETAL1", "invalid magic/version/size");
    Check(Digest(bytes.substr(72)) == bytes.substr(8, 64), "payload checksum mismatch");
    size_t offset = 72;
    Check(Take(bytes, offset) == Profile(), "stale toolchain/profile; regenerate offline shader package");
    MetalShaderArtifact result;
    result.Name = Take(bytes, offset);
    const auto slots = Take(bytes, offset);
    Check(slots == "16" || slots == "32", "invalid variant");
    result.TextureSlots = slots == "16" ? 16 : 32;
    result.SourceIdentity = Take(bytes, offset); result.Producer = Take(bytes, offset);
    for (auto& stage : result.Stages)
    {
        auto spirv = Take(bytes, offset);
        Check(spirv.size() % 4 == 0, "unaligned SPIR-V");
        stage.Spirv.resize(spirv.size() / 4);
        std::memcpy(stage.Spirv.data(), spirv.data(), spirv.size());
        stage.Library = Take(bytes, offset);
    }
    Check(offset == bytes.size(), "trailing data");
    result.Validate();
    return result;
}
std::string ReadMetalShaderFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Check(bool(input), "cannot open " + path.u8string());
    const auto size = input.tellg();
    Check(size >= 0 && size <= static_cast<std::streamoff>(Limit), "invalid file size: " + path.u8string());
    std::string bytes(static_cast<size_t>(size), '\0');
    input.seekg(0); input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Check(bool(input), "cannot read " + path.u8string());
    return bytes;
}
void WriteMetalShaderFile(const std::filesystem::path& path, const std::string& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    // Unique temporary files keep independent processes from truncating another
    // writer's entry. Rename publishes complete bytes on the same filesystem.
    std::string pattern = path.u8string() + ".tmp-XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end()); temporary.push_back('\0');
    const int fd = mkstemp(temporary.data());
    Check(fd >= 0, "cannot create temporary cache entry: " + path.u8string());
    auto* output = fdopen(fd, "wb");
    if (!output) { close(fd); std::filesystem::remove(temporary.data()); Check(false, "cannot open temporary cache stream"); }
    const bool written = fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
    const bool closed = fclose(output) == 0;
    try
    {
        Check(written && closed, "cannot write " + path.u8string());
        std::filesystem::rename(temporary.data(), path);
    }
    catch (...) { std::filesystem::remove(temporary.data()); throw; }
}
}
