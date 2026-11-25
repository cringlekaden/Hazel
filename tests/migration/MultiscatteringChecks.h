#pragma once
#include "MultiscatteringReference.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Hazel/Core/Resources.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

inline void MultiscatteringChecks(const std::filesystem::path &resources)
{
    using namespace Hazel;
    auto require = [](bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    };
    const auto image = DecodeLightingImage(ReadAssetBytes(
        resources / RuntimeBRDFResource, LightingArtifactByteLimit));
    const auto packed = PackBRDFTable(image);
    double maximumAverageError = 0, maximumFurnaceError = 0, maximumReciprocity = 0;
    for (unsigned y = 0; y < image.Side; ++y)
    {
        const auto average = ReferenceAverageRuntimeBRDF(image, .045+.955*y/double(image.Side-1));
        for (unsigned x = 0; x < image.Side; ++x)
        {
            const size_t p = size_t(y) * image.Side + x;
            require(std::memcmp(&packed[p*4], &image.Mips[0][p*2], 2*sizeof(float)) == 0,
                    "Multiscattering packing changed runtime A/B bytes");
            maximumAverageError = std::max(maximumAverageError,
                glm::length(average - glm::dvec2(packed[p*4+2],packed[p*4+3])));
        }
    }
    require(maximumAverageError < 6e-8, "Packed hemispherical means differ from independent quadrature");
    for (double rough : {0., .044, .045, .046, .1, .25, .5, .75, 1.})
        for (double mu : {.0001, .01, .05, .25, .5, .999, 1.})
        {
            const auto ab = ReferenceSampleRuntimeBRDF(image,mu,std::max(.045,rough));
            const auto average = ReferenceAverageRuntimeBRDF(image,std::max(.045,rough));
            const auto white = ReferenceMultiscatteringEnergy(glm::dvec3(1),ab,average);
            maximumFurnaceError = std::max(maximumFurnaceError, std::abs(white.Reflected.x-1));
            require(std::abs(white.Reflected.x-1) < .003, "Perfect-reflector furnace failed");
            for (auto f0 : {glm::dvec3(.04),glm::dvec3(.02),glm::dvec3(.955,.638,.538),glm::dvec3(.95,.64,.15)})
            {
                const auto energy = ReferenceMultiscatteringEnergy(f0,ab,average);
                for (int c=0;c<3;++c)
                    require(std::isfinite(energy.Reflected[c]) && energy.Reflected[c]>=0 &&
                            energy.Reflected[c]+energy.Residual<=1.003,
                            "Complementary diffuse/specular energy budget failed");
                if(f0.x>f0.z)require(energy.C.x>energy.C.z,"Colored multibounce absorption lost");
            }
        }
    for (double rough : {.045, .25, .5, 1.})
        for (double mu : {.01, .25, .8, 1.})
        {
            glm::dvec3 v(std::sqrt(1-mu*mu),0,mu),l=glm::normalize(glm::dvec3(.2,.3,1));
            auto forward=OracleMultiscatteringBRDF({.955,.638,.538},.5,rough,{0,0,1},v,l)/l.z;
            auto reverse=OracleMultiscatteringBRDF({.955,.638,.538},.5,rough,{0,0,1},l,v)/v.z;
            maximumReciprocity=std::max(maximumReciprocity,glm::length(forward-reverse));
        }
    require(maximumReciprocity < 1e-9,"Intrinsic multiscattering BRDF is not reciprocal");
    require(std::abs(IBLReferenceIntegral(1,1).x+IBLReferenceIntegral(1,1).y-(1-std::log(2.)))<.00002,
            "Historical single-scattering analytic reference changed");
    const auto roughNormal=MultiscatteringIntegral(1,1,512);
    require(std::abs(roughNormal.x+roughNormal.y-(1-std::log(2.)))<.00002,
            "Boundary-split reference lost analytic single-scattering energy");
    for(double r:{.5,1.})for(double mu:{.25,.5,1.})
        require(glm::length(MultiscatteringAlbedo(mu,r)-IBLReferenceIntegral(mu,r,256))<.0001,
                "Boundary-split and legacy quadratures disagree in ordinary domain");
    double maximumDirectEnergyError=0;
    for(double rough:{.045,.046,.047,.05,.1,.125,.25,.5,.75,.999,1.})
        for(double mu:{.00001,.0001,.0005,.001,.002,.005,.01,.05,.1,.25,.5,.9,1.}) {
            const auto physical=MultiscatteringAlbedo(mu,rough);
            const auto sampled=ReferenceSampleRuntimeBRDF(image,mu,rough);
            // Integrating the separable lobes against their own interpolant
            // averages leaves only actual analytic SS minus LUT SS error.
            const double perfect=1+physical.x+physical.y-sampled.x-sampled.y;
            const double white=1+.04*(physical.x-sampled.x)+physical.y-sampled.y;
            maximumDirectEnergyError=std::max({maximumDirectEnergyError,std::abs(perfect-1),std::abs(white-1)});
            require(std::abs(perfect-1)<=.003 && std::abs(white-1)<=.003,
                    "Analytic direct energy differs from the LUT compensation budget");
        }
    std::cout<<"PASS: independent analytic direct energy sweep max "<<maximumDirectEnergyError<<'\n';
    std::cout << "PASS: Multiscattering_v2 independent FP64/packed sampler averages max "
              << maximumAverageError << "; perfect furnace max " << maximumFurnaceError
              << "; reciprocity max " << maximumReciprocity
              << "; colored absorption, residual budgets, floor/grazing endpoints, immutable runtime A/B\n";
}
