#pragma once
#include "Hazel/Assets/Environment.h"
#include <functional>
namespace Hazel::EnvironmentBake
{
// Producer-only solid-angle proposal. This is not a runtime texture or a final
// roughness mip pyramid. All estimators evaluate the original linear source.
class RadianceSampler
{
  public:
    struct Sample { glm::dvec3 Direction, Radiance; double PDF; };
    explicit RadianceSampler(const RadianceImage &image, const std::function<bool()> &cancel = {});
    glm::dvec3 Evaluate(glm::dvec3 direction) const;
    double PDF(glm::dvec3 direction) const;
    std::vector<Sample> Samples(unsigned count) const;
    bool Constant() const { return m_Constant; }
    glm::dvec3 ConstantValue() const;
  private:
    const RadianceImage &m_Image;
    std::vector<double> m_CDF;
    double m_Total = 0;
    bool m_Constant = true;
    double Area(unsigned y) const;
};
}
