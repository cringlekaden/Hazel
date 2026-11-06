#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <stdexcept>
namespace Hazel {
// Linear Rec.709 incident RGB, renderer-relative intensity; no photometric units.
struct DirectionalLight {
    glm::vec3 Color{1};
    float Intensity=1;
    bool Enabled=true;
    void Validate() const {
        for(int i=0;i<3;++i)if(!std::isfinite(Color[i])||Color[i]<0||Color[i]>1)throw std::runtime_error("Light color requires linear RGB in [0,1]");
        if(!std::isfinite(Intensity)||Intensity<0||Intensity>1000)throw std::runtime_error("Directional intensity requires [0,1000]");
    }
};
inline constexpr float DirectBRDFRoughnessFloor=.045f; // Direct_v1: single scattering GGX / correlated Smith.
}
