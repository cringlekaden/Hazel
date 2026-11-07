#pragma once
#include "IBLQualityMath.h"
static void IBLQualityBakeChecks()
{
    using namespace Hazel;
    RadianceImage source;source.Width=128;source.Height=64;source.RGB.resize(128*64*3,.01f);
    for(int c=0;c<3;++c)source.RGB[(size_t(25)*128+69)*3+c]=10000;
    auto prepared=EnvironmentBake::Prepare(source,{8,64,16,1024});
    IBLSourceQuadrature coarse(source,4),fine(source,8);
    double maxError=0,maxConvergence=0;
    for(auto image:{&prepared.Irradiance,&prepared.Specular})
        for(unsigned mip=0;mip<image->Mips.size();++mip)
        {
            if(image->Kind==LightingImageKind::Specular && mip==0)continue;
            unsigned side=image->Side>>mip;
            const bool diffuse=image->Kind==LightingImageKind::Irradiance;
            double rough=diffuse?1.:double(mip)/(image->Mips.size()-1);
            std::vector<double> errors;double peak=0,squares=0,expectedSum=0,actualSum=0,delta=0;
            for(unsigned face=0;face<6;++face)
                for(unsigned x:{0u,side/2,side-1})
                {
                    unsigned y=side/2;
                    auto direction=CubeDirection(face,(x+.5)/side,(y+.5)/side);
                    auto a=coarse.Integrate(direction,rough,diffuse),b=fine.Integrate(direction,rough,diffuse);
                    size_t offset=((size_t(face)*side+y)*side+x)*3;
                    for(int c=0;c<3;++c){double error=std::abs(image->Mips[mip][offset+c]-b[c]);peak=std::max(peak,b[c]);errors.push_back(error);squares+=error*error;expectedSum+=b[c];actualSum+=image->Mips[mip][offset+c];delta=std::max(delta,std::abs(a[c]-b[c]));}
                }
            std::sort(errors.begin(),errors.end());double rms=std::sqrt(squares/errors.size())/peak,p99=errors[size_t(.99*(errors.size()-1))]/peak,outlier=errors.back()/peak;
            std::cout<<"IBL quality "<<(diffuse?"diffuse":"specular")<<" r="<<rough<<" RMS="<<rms<<" p99="<<p99<<" max="<<outlier<<" quadrature="<<delta/peak<<" energy="<<(actualSum-expectedSum)/expectedSum<<'\n';
            Check(delta/peak<.002,"Tiny-emitter independent source quadrature did not converge");
            Check(rms<.01 && p99<.03 && outlier<.05,"Tiny-emitter convolution spikes or lobe integration error");
            Check(std::abs(actualSum-expectedSum)/expectedSum<.01,"Tiny-emitter convolved energy error");
            maxError=std::max(maxError,outlier);maxConvergence=std::max(maxConvergence,delta/peak);
        }
    // Exact cell solid-angle integration over all six faces, not an average
    // of a few directions. A normalized rotational convolution preserves flux.
    glm::dvec3 sourceFlux(0);for(const auto &q:fine.Points)sourceFlux+=q.Energy;
    double maximumFluxError=0;
    auto corner=[](double x,double y){return std::atan2(x*y,std::sqrt(1+x*x+y*y));};
    for(auto image:{&prepared.Irradiance,&prepared.Specular})
        for(unsigned mip=0;mip<image->Mips.size();++mip)
        {
            unsigned side=image->Side>>mip;glm::dvec3 flux(0);
            for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x)
            {
                double x0=2.*x/side-1,x1=2.*(x+1)/side-1,y0=2.*y/side-1,y1=2.*(y+1)/side-1;
                double area=corner(x1,y1)-corner(x0,y1)-corner(x1,y0)+corner(x0,y0);
                for(unsigned face=0;face<6;++face){size_t at=((size_t(face)*side+y)*side+x)*3;flux+=glm::dvec3(image->Mips[mip][at],image->Mips[mip][at+1],image->Mips[mip][at+2])*area;}
            }
            if(image->Kind==LightingImageKind::Irradiance)flux/=std::acos(-1.);
            double error=glm::length(flux-sourceFlux)/glm::length(sourceFlux);
            // Mip zero is the unchanged point endpoint: its spatial resampling
            // is measured separately from convolution energy.
            if(image->Kind==LightingImageKind::Specular && mip==0)
                std::cout<<"IBL sharp endpoint spatial resampling flux error "<<error<<'\n';
            else {Check(error<.01,"Convolved full-sphere radiance energy changed");maximumFluxError=std::max(maximumFluxError,error);}
        }
    std::cout<<"IBL full-sphere convolved flux max error "<<maximumFluxError<<'\n';
    // The two codecs declare a complete chain for their own version. Unknown
    // future versions and partial chains still reject before GPU publication.
    auto bytes=EncodeLightingImage(prepared.Specular);
    Check(DecodeLightingImage(bytes).EncodingVersion==2 && (prepared.Specular.Side>>(prepared.Specular.Mips.size()-1))==4,"Tail-four portable contract");
    auto invalid=prepared.Specular;invalid.Mips.pop_back();bool rejected=false;try{invalid.Validate();}catch(const std::exception&){rejected=true;}Check(rejected,"Partial declared specular chain accepted");
    std::cout<<"PASS: IBLQuality_v2 independent finite-emitter diffuse/specular multi-level/outlier/energy checks; max "<<maxError<<" convergence "<<maxConvergence<<'\n';
}
