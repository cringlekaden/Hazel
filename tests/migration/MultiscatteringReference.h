#pragma once
#include "DirectBRDFReference.h"
#include "IBLReferenceMath.h"
#include <map>
#include <utility>
#include <stdexcept>
#include <string>

// Independent FP64 quadrature of the authored GGX BRDF, not the production
// LUT producer or shader. Keep the historical single-scattering oracle intact.
// Boundary-split GGX NDF quadrature resolves the small-loss regime independently
// of the offline producer's VNDF grazing integration.
inline glm::dvec2 MultiscatteringIntegral(double mu, double rough, unsigned count)
{
    // Independent NDF-domain reference: solve the reflected upper-hemisphere
    // boundary per azimuth, then integrate the analytic correlated BRDF with
    // the NDF change of variables. Unlike the production grazing VNDF rule,
    // no rejected-light discontinuity or unresolved disk-edge tail remains.
    const double alpha=std::pow(std::max(.045,rough),2),pi=std::acos(-1.);
    const double sine=std::sqrt(1-mu*mu),a2=alpha*alpha;
    glm::dvec2 result(0);
    const auto nodes=IBLReferenceNodes(count);
    for(unsigned j=0;j<4*count;++j)
    {
        const double cp=std::cos(2*pi*(j+.5)/(4*count)),c=sine*cp;
        const double boundary=c>=0?(c+std::hypot(c,mu))/mu:mu/(std::hypot(c,mu)-c);
        const double cap=boundary*boundary/(a2+boundary*boundary);
        for(auto node:nodes)
        {
            const double u=cap*node.x*(2-node.x),tangent=alpha*std::sqrt(u/(1-u));
            const double nh=1/std::sqrt(1+tangent*tangent),vh=(mu+c*tangent)*nh;
            const double nl=2*vh*nh-mu;
            if(nl<=0)continue;
            const double visibility=.5/(nl*std::sqrt(mu*mu*(1-a2)+a2)+mu*std::sqrt(nl*nl*(1-a2)+a2));
            const double weight=visibility*nl*4*vh/nh,fc=std::pow(1-vh,5);
            result+=glm::dvec2(1-fc,fc)*(weight*node.y*cap*2*(1-node.x)/(4*count));
        }
    }
    return result;
}
inline glm::dvec2 MultiscatteringAlbedo(double mu, double rough)
{
    static std::map<std::pair<double, double>, glm::dvec2> cache;
    const auto key = std::make_pair(mu, rough);
    const auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    auto previous = MultiscatteringIntegral(mu, rough, 128);
    for (unsigned order = 256; order <= 4096; order *= 2)
    {
        auto refined = MultiscatteringIntegral(mu, rough, order);
        const double missing=std::max(0.,1-refined.x-refined.y);
        if (glm::length(refined - previous) < 1e-7 &&
            std::abs(refined.x+refined.y-previous.x-previous.y)<std::max(1e-9,missing*1e-3))
            return cache.emplace(key, refined).first->second;
        previous = refined;
    }
    throw std::runtime_error("Independent GGX albedo quadrature did not converge: NoV="+std::to_string(mu)+" rough="+std::to_string(rough));
}
inline glm::dvec2 MultiscatteringAverage(double rough)
{
    static std::map<double, glm::dvec2> cache;
    const auto found = cache.find(rough);
    if (found != cache.end()) return found->second;
    glm::dvec2 average(0);
    for (auto node : IBLReferenceNodes(64))
        average += MultiscatteringAlbedo(node.x, rough) * (2 * node.x * node.y);
    return cache.emplace(rough, average).first->second;
}
struct MultiscatteringEnergy
{
    glm::dvec3 C, Reflected;
    double Residual = 0, AverageResidual = 0;
};
inline MultiscatteringEnergy ReferenceMultiscatteringEnergy(glm::dvec3 f0,
    glm::dvec2 ab, glm::dvec2 average)
{
    const double e = ab.x + ab.y, ea = average.x + average.y;
    const auto f = f0 + (1.0 - f0) / 21.;
    MultiscatteringEnergy result;
    result.C = f * f * ea / (1.0 - f * (1.0 - ea));
    result.Reflected = f0 * ab.x + ab.y + result.C * std::max(0., 1 - e);
    const double maximum = std::max({f0.x, f0.y, f0.z});
    const double fm = maximum + (1 - maximum) / 21.;
    const double c = fm * fm * ea / (1 - fm * (1 - ea));
    result.Residual = std::max(0., 1 - (maximum * ab.x + ab.y + c * std::max(0., 1 - e)));
    result.AverageResidual = std::max(0., 1 - (maximum * average.x + average.y + c * (1 - ea)));
    return result;
}
inline glm::dvec3 ReferenceMultiscatteringBRDF(glm::dvec3 base, double metal, double rough,
    glm::dvec3 n, glm::dvec3 v, glm::dvec3 l, glm::dvec2 abv, glm::dvec2 abl, glm::dvec2 av)
{
    const double nv = glm::dot(n, v), nl = glm::dot(n, l), pi = std::acos(-1.);
    if (nv <= 0 || nl <= 0) return {};
    const auto f0 = glm::mix(glm::dvec3(.04), base, metal);
    const auto ev = ReferenceMultiscatteringEnergy(f0, abv, av);
    const auto el = ReferenceMultiscatteringEnergy(f0, abl, av);
    const auto h = glm::normalize(v + l);
    const auto fresnel = f0 + (1.0 - f0) * std::pow(1 - glm::dot(v, h), 5);
    auto single = OracleBRDF(base, metal, rough, n, v, l) - (1 - metal) * (1.0 - fresnel) * base * (nl / pi);
    const double missingAverage = std::max(0., 1 - av.x - av.y);
    if (missingAverage > 0)
        single += ev.C * (std::max(0., 1 - abv.x - abv.y) * std::max(0., 1 - abl.x - abl.y) * nl / (pi * missingAverage));
    if (ev.AverageResidual > 0)
        single += (1 - metal) * base * (el.Residual * ev.Residual * nl / (pi * ev.AverageResidual));
    return single;
}
inline glm::dvec3 OracleMultiscatteringBRDF(glm::dvec3 base, double metal, double rough,
    glm::dvec3 n, glm::dvec3 v, glm::dvec3 l)
{
    if(glm::dot(n,v)<=0 || glm::dot(n,l)<=0)return {};
    return ReferenceMultiscatteringBRDF(base,metal,rough,n,v,l,
        MultiscatteringAlbedo(glm::dot(n,v),rough),MultiscatteringAlbedo(glm::dot(n,l),rough),
        MultiscatteringAverage(rough));
}
// Independent CPU texture sampling plus numerical Gauss integration. This oracle
// isolates LUT representation error from the quadrature model above.
template <typename Image>
inline glm::dvec2 ReferenceSampleBRDF(const Image &image, double mu, double rough)
{
    const double px = mu * image.Side - .5, py = rough * image.Side - .5;
    const int x = int(std::floor(px)), y = int(std::floor(py));
    auto at = [&](int xx, int yy) {
        const size_t p = (size_t(std::clamp(yy, 0, int(image.Side) - 1)) * image.Side +
                          size_t(std::clamp(xx, 0, int(image.Side) - 1))) * 2;
        return glm::dvec2(image.Mips[0][p], image.Mips[0][p + 1]);
    };
    return glm::mix(glm::mix(at(x,y), at(x+1,y), px-x),
                    glm::mix(at(x,y+1), at(x+1,y+1), px-x), py-y);
}
template <typename Image>
inline glm::dvec2 ReferenceAverageBRDF(const Image &image, double rough)
{
    glm::dvec2 average(0);
    const auto nodes = IBLReferenceNodes(3);
    double left = 0;
    for (unsigned interval = 0; interval <= image.Side; ++interval)
    {
        const double right = interval == image.Side ? 1 : (interval + .5) / image.Side;
        for (auto node : nodes)
        {
            const double mu = left + (right-left) * node.x;
            average += ReferenceSampleBRDF(image, mu, rough) * (2 * mu * node.y * (right-left));
        }
        left = right;
    }
    return average;
}
template <typename Image>
inline MultiscatteringEnergy ReferenceTableEnergy(const Image &image, glm::dvec3 f0,
    double mu, double rough)
{
    const double evaluated = std::max(.045,rough);
    return ReferenceMultiscatteringEnergy(f0, ReferenceSampleBRDF(image,mu,evaluated),
                                          ReferenceAverageBRDF(image,evaluated));
}

template <typename Image>
inline glm::dvec3 OracleTableMultiscatteringBRDF(const Image &image, glm::dvec3 base,
    double metal, double rough, glm::dvec3 n, glm::dvec3 v, glm::dvec3 l)
{
    if(glm::dot(n,v)<=0 || glm::dot(n,l)<=0)return {};
    const double r=std::max(.045,rough);
    return ReferenceMultiscatteringBRDF(base,metal,rough,n,v,l,
        ReferenceSampleBRDF(image,glm::dot(n,v),r),ReferenceSampleBRDF(image,glm::dot(n,l),r),
        ReferenceAverageBRDF(image,r));
}
// RuntimeBRDF_v2 endpoint grid: x=NoV^(1/4), y=(r-.045)/.955.
// Kept separate from historical texture sampling so frozen SS studies stay independent.
template <typename Image>
inline glm::dvec2 ReferenceSampleRuntimeBRDF(const Image &image, double mu, double rough)
{
    const double px=std::pow(std::clamp(mu,0.,1.),.25)*(image.Side-1);
    const double py=(std::clamp(rough,.045,1.)-.045)/.955*(image.Side-1);
    const int x=int(std::floor(px)),y=int(std::floor(py));
    auto at=[&](int xx,int yy) {
        const size_t p=(size_t(std::clamp(yy,0,int(image.Side)-1))*image.Side+
                        size_t(std::clamp(xx,0,int(image.Side)-1)))*2;
        return glm::dvec2(image.Mips[0][p],image.Mips[0][p+1]);
    };
    return glm::mix(glm::mix(at(x,y),at(x+1,y),px-x),
                    glm::mix(at(x,y+1),at(x+1,y+1),px-x),py-y);
}
template <typename Image>
inline glm::dvec2 ReferenceAverageRuntimeBRDF(const Image &image,double rough)
{
    glm::dvec2 average(0);
    for(unsigned i=0;i+1<image.Side;++i)
        for(auto node:IBLReferenceNodes(5)) {
            const double t=(i+node.x)/(image.Side-1),mu=t*t*t*t;
            average+=ReferenceSampleRuntimeBRDF(image,mu,rough)*(8*std::pow(t,7)*node.y/(image.Side-1));
        }
    return average;
}
template <typename Image>
inline MultiscatteringEnergy ReferenceRuntimeTableEnergy(const Image &image,glm::dvec3 f0,
    double mu,double rough)
{
    return ReferenceMultiscatteringEnergy(f0,ReferenceSampleRuntimeBRDF(image,mu,rough),
        ReferenceAverageRuntimeBRDF(image,rough));
}
template <typename Image>
inline glm::dvec3 OracleRuntimeMultiscatteringBRDF(const Image &image,glm::dvec3 base,
    double metal,double rough,glm::dvec3 n,glm::dvec3 v,glm::dvec3 l)
{
    if(glm::dot(n,v)<=0 || glm::dot(n,l)<=0)return {};
    return ReferenceMultiscatteringBRDF(base,metal,rough,n,v,l,
        ReferenceSampleRuntimeBRDF(image,glm::dot(n,v),rough),
        ReferenceSampleRuntimeBRDF(image,glm::dot(n,l),rough),ReferenceAverageRuntimeBRDF(image,rough));
}
