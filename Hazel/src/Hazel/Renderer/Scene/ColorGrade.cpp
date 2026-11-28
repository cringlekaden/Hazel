#include "hzpch.h"
#include "ColorGrade.h"
#include "Hazel/Core/FileDocument.h"
#include <glm/gtc/packing.hpp>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Hazel
{
ColorGradeLUT LoadColorGradeLUT(const std::filesystem::path& path)
{
    constexpr uint64_t fileLimit = 16 * 1024 * 1024;
    if (std::filesystem::file_size(path) > fileLimit)
        throw std::runtime_error("Camera LUT exceeds 16 MiB");
    std::istringstream file(FileDocument::Read(path));
    std::vector<uint64_t> pixels;
    unsigned size = 0;
    glm::vec3 lower(0), upper(1);
    bool minRead = false, maxRead = false, resolveRangeRead = false;
    const auto invalid = [] { throw std::runtime_error("Invalid or unsupported 3D .cube LUT"); };
    for (std::string text; std::getline(file, text);) {
        if (const auto comment = text.find('#'); comment != std::string::npos)
            text.resize(comment);
        std::istringstream line(text);
        line.imbue(std::locale::classic());
        std::string token, extra;
        if (!(line >> token)) continue;
        if (token == "TITLE" && pixels.empty()) continue;
        if (token == "LUT_3D_SIZE" && !size) {
            if (!(line >> size) || size < 2 || size > 64 || line >> extra) invalid();
            pixels.reserve(size_t(size) * size * size);
            continue;
        }
        if ((token == "DOMAIN_MIN" || token == "DOMAIN_MAX") && pixels.empty() && !resolveRangeRead) {
            auto& seen = token == "DOMAIN_MIN" ? minRead : maxRead;
            auto& bound = token == "DOMAIN_MIN" ? lower : upper;
            if (seen || !(line >> bound.x >> bound.y >> bound.z) || line >> extra) invalid();
            seen = true;
            continue;
        }
        if (token == "LUT_3D_INPUT_RANGE" && pixels.empty() && !minRead && !maxRead && !resolveRangeRead) {
            float minimum, maximum;
            if (!(line >> minimum >> maximum) || line >> extra) invalid();
            lower = glm::vec3(minimum);
            upper = glm::vec3(maximum);
            resolveRangeRead = true;
            continue;
        }
        glm::vec3 rgb;
        line.clear();
        line.seekg(0);
        if (!size || pixels.size() >= size_t(size) * size * size ||
            !(line >> rgb.x >> rgb.y >> rgb.z) || line >> extra) invalid();
        for (int c = 0; c < 3; ++c)
            if (!(rgb[c] >= 0 && rgb[c] <= 1)) invalid();
        pixels.push_back(glm::packHalf4x16(glm::vec4(rgb, 1)));
    }
    if (!size || pixels.size() != size_t(size) * size * size) invalid();
    for (int c = 0; c < 3; ++c)
        if (!(lower[c] >= -16 && upper[c] <= 16 && upper[c] - lower[c] >= .0001f)) invalid();

    // .cube stores red fastest, then green, then blue. Keep each blue plane
    // contiguous within each atlas row, so hardware interpolates red and green.
    std::vector<uint64_t> atlas(pixels.size());
    for (unsigned blue = 0; blue < size; ++blue)
        for (unsigned green = 0; green < size; ++green)
            std::copy_n(pixels.begin() + (blue * size + green) * size, size,
                atlas.begin() + (green * size + blue) * size);
    TextureSpecification spec;
    spec.Width = size * size;
    spec.Height = size;
    spec.Format = ImageFormat::RGBA16F;
    spec.GenerateMips = false;
    spec.MinFilter = spec.MagFilter = TextureFilter::Linear;
    spec.WrapS = spec.WrapT = TextureWrap::ClampToEdge;
    auto texture = Texture2D::Create(spec);
    texture->SetData(atlas.data(), uint32_t(atlas.size() * sizeof(atlas.front())));
    const auto scale = 1.f / (upper - lower);
    return {std::move(texture), scale, -lower * scale, size};
}
}
