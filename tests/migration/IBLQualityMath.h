#pragma once
#include "Hazel/Assets/Environment.h"
#include <cmath>
#include <algorithm>
// Independent solid-angle source quadrature: no proposal CDF, Hammersley/MIS
// loops, production sampling helpers or prepared textures in the oracle.
struct IBLSourceQuadrature
{
    struct Point { glm::dvec3 Direction, Energy; };
    std::vector<Point> Points;
    explicit IBLSourceQuadrature(const Hazel::RadianceImage &source, unsigned subdivision)
    {
        const double pi=std::acos(-1.);
        auto pixel=[&](int x,int y){x=(x%int(source.Width)+int(source.Width))%int(source.Width);y=std::clamp(y,0,int(source.Height)-1);size_t p=(size_t(y)*source.Width+x)*3;return glm::dvec3(source.RGB[p],source.RGB[p+1],source.RGB[p+2]);};
        const unsigned w=source.Width*subdivision,h=source.Height*subdivision;
        Points.reserve(size_t(w)*h);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
        {
            double u=(x+.5)/w,v=(y+.5)/h,theta=pi*v,phi=2*pi*(u-.5);
            double sx=u*source.Width-.5,sy=v*source.Height-.5;int ix=int(std::floor(sx)),iy=int(std::floor(sy));
            auto light=glm::mix(glm::mix(pixel(ix,iy),pixel(ix+1,iy),sx-ix),glm::mix(pixel(ix,iy+1),pixel(ix+1,iy+1),sx-ix),sy-iy);
            double solidAngle=2*pi/w*(std::cos(pi*y/h)-std::cos(pi*(y+1)/h));
            Points.push_back({{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)},light*solidAngle});
        }
    }
    glm::dvec3 Integrate(glm::dvec3 normal,double roughness,bool diffuse) const
    {
        const double pi=std::acos(-1.),a2=std::pow(std::max(.045,roughness),4);
        // Independent 1D midpoint integration of the normalization (not the
        // production closed form). Very sharp lobes use the GGX CDF variable.
        double normalization=0;
        const unsigned n=32768;
        if(!diffuse)for(unsigned i=0;i<n;++i){double h2=(1-(i+.5)/n)/(1+(a2-1)*(i+.5)/n);normalization+=std::max(0.,2*h2-1)/n;}
        glm::dvec3 sum(0);
        for(const auto &p:Points)
        {
            double cosine=glm::dot(normal,p.Direction);if(cosine<=0)continue;
            double weight=cosine;
            if(!diffuse){double nh2=(1+cosine)/2,den=(1-nh2)+a2*nh2;double distribution=a2/(pi*den*den);weight*=distribution/(4*normalization);}
            sum+=p.Energy*weight;
        }
        return sum;
    }
};
