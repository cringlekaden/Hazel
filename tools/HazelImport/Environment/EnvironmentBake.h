#pragma once
#include "Hazel/Assets/Environment.h"
#include <functional>
namespace Hazel::EnvironmentBake
{
using Cancel = std::function<bool()>;
// Format-independent preparation consumes normalized linear radiance only.
struct LightingSet
{
    LightingImage Background, Irradiance, Specular;
};
LightingSet Prepare(const RadianceImage &source, const EnvironmentProfile &profile,
                    const Cancel &cancel = {});
LightingImage IntegrateBRDF(const Cancel &cancel = {});
std::string Producer();
std::string LUTKey();
// Native producer adapter/publication. Runtime never links this module.
void PrepareImage(const std::filesystem::path &assets, const std::filesystem::path &environment,
                  const Cancel &cancel = {});
} // namespace Hazel::EnvironmentBake
