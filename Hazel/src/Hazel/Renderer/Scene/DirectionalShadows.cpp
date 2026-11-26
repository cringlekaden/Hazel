#include "hzpch.h"
#include "DirectionalShadows.h"
#include "SceneSubmission.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Hazel {
void DirectionalShadowSettings::Validate() const
{
    if((Resolution!=256 && Resolution!=512 && Resolution!=1024) ||
       !std::isfinite(Distance) || Distance<.1f || Distance>10000 ||
       !std::isfinite(SplitLambda) || SplitLambda<0 || SplitLambda>1 ||
       !std::isfinite(NormalBias) || NormalBias<0 || NormalBias>2 ||
       !std::isfinite(DepthBias) || DepthBias<0 || DepthBias>1 ||
       !std::isfinite(Blend) || Blend<.01f || Blend>.25f)
        throw std::runtime_error("CSM requires resolution 256/512/1024, distance .1..10000, split 0..1, normal bias 0..2, depth bias 0..1 texels and blend .01...25");
}
bool ShadowCasterVisible(const glm::vec3& minimum,const glm::vec3& maximum,const glm::mat4& clip)
{
    std::array<glm::vec4,8> corners;
    for(unsigned i=0;i<8;++i)corners[i]=clip*glm::vec4((i&1)?maximum.x:minimum.x,
        (i&2)?maximum.y:minimum.y,(i&4)?maximum.z:minimum.z,1);
    for(unsigned axis=0;axis<3;++axis)for(int side:{-1,1}) {
        bool outside=true;
        for(const auto& p:corners)outside &= float(side)*p[axis]>p.w;
        if(outside)return false;
    }
    return true;
}
DirectionalShadowData FitDirectionalShadows(const SceneSubmission& frame)
{
    const auto& settings=frame.Presentation.Shadows;settings.Validate();
    DirectionalShadowData result;
    if(!settings.Enabled || !frame.Light)return result;
    const glm::dmat4 vp(frame.Camera.ViewProjection);
    const auto determinant=glm::determinant(vp);
    const glm::dvec3 eye(frame.Camera.Position),forward=-glm::dvec3(frame.Camera.ToCamera);
    const glm::dvec3 light(frame.Light->ToLight);
    if(!std::isfinite(determinant) || std::abs(determinant)<1e-12 ||
       !std::isfinite(glm::length(eye)) || !std::isfinite(glm::length(forward)) ||
       std::abs(glm::length(forward)-1)>1e-4 || !std::isfinite(glm::length(light)) ||
       std::abs(glm::length(light)-1)>1e-4)
        throw std::runtime_error("CSM requires finite invertible VP, eye and unit camera/light directions");
    const auto inverse=glm::inverse(vp);
    std::array<glm::dvec3,4> nearCorners,farCorners;
    double nearDepth=0,farDepth=0;
    for(unsigned i=0;i<4;++i) {
        for(unsigned end=0;end<2;++end) {
            const auto h=inverse*glm::dvec4((i&1)?1:-1,(i&2)?1:-1,end?1:-1,1);
            if(!std::isfinite(h.w) || std::abs(h.w)<1e-12)
                throw std::runtime_error("CSM requires finite near/far frustum planes");
            const auto p=glm::dvec3(h)/h.w;
            if(!std::isfinite(glm::length(p)))throw std::runtime_error("Nonfinite CSM frustum");
            (end?farCorners:nearCorners)[i]=p;
            (end?farDepth:nearDepth)+=glm::dot(p-eye,forward)*.25;
        }
    }
    // Existing public cameras are finite, parallel near/far planes. Reject
    // oblique/reversed inputs explicitly instead of silently mispartitioning.
    if(!std::isfinite(nearDepth) || !std::isfinite(farDepth) || farDepth<=nearDepth ||
       (!frame.Camera.Orthographic && nearDepth<=0))
        throw std::runtime_error("CSM requires standard-Z finite camera depth planes");
    for(unsigned i=0;i<4;++i)
        if(std::abs(glm::dot(nearCorners[i]-eye,forward)-nearDepth)>std::max(1e-4,std::abs(nearDepth)*1e-3) ||
           std::abs(glm::dot(farCorners[i]-eye,forward)-farDepth)>std::max(1e-4,std::abs(farDepth)*1e-3))
            throw std::runtime_error("CSM does not support oblique camera planes");
    const double end=std::min(farDepth,double(settings.Distance));
    if(end<=nearDepth)throw std::runtime_error("CSM distance must exceed camera near depth");
    const auto right=glm::normalize(glm::cross(std::abs(light.y)<.95?glm::dvec3(0,1,0):glm::dvec3(1,0,0),light));
    const auto up=glm::cross(light,right);
    std::array<double,5> splits{nearDepth};
    for(unsigned i=1;i<=4;++i) {
        const double t=double(i)/4,uniform=nearDepth+(end-nearDepth)*t;
        splits[i]=frame.Camera.Orthographic?uniform:uniform*(1-settings.SplitLambda)+
            nearDepth*std::pow(end/nearDepth,t)*settings.SplitLambda;
        result.Splits[i-1]=float(splits[i]);
    }
    for(unsigned cascade=0;cascade<4;++cascade) {
        // Next cascade covers the preceding interval's blend band too.
        const double begin=cascade?splits[cascade]-(splits[cascade]-splits[cascade-1])*settings.Blend:nearDepth;
        std::array<glm::dvec3,8> corners;
        glm::dvec3 centre(0);
        for(unsigned i=0;i<8;++i) {
            const double depth=(i&4)?splits[cascade+1]:begin;
            corners[i]=glm::mix(nearCorners[i&3],farCorners[i&3],(depth-nearDepth)/(farDepth-nearDepth));
            centre+=corners[i]/8.;
        }
        double radius=0;
        for(const auto& p:corners)radius=std::max(radius,glm::length(p-centre));
        // Two texels for filtering, half for snap, plus maximum receiver offset.
        radius=std::ceil(radius*16)/16;
        radius/=1-2*(2.5+settings.NormalBias)/double(settings.Resolution);
        const double texel=2*radius/settings.Resolution;
        const double x=std::round(glm::dot(centre,right)/texel)*texel;
        const double y=std::round(glm::dot(centre,up)/texel)*texel;
        double lo=std::numeric_limits<double>::max(),hi=-lo;
        for(const auto& p:corners){const auto z=glm::dot(light,p);lo=std::min(lo,z);hi=std::max(hi,z);}
        // Include all overlapping scene casters, even beyond the camera far
        // plane or behind the eye. Bounds cannot reject a true intersection.
        for(const auto& draw:frame.ShadowCasters) {
            const auto& bounds=draw.Geometry->Geometry.Bounds;
            glm::dvec3 minimum(std::numeric_limits<double>::max()),maximum(-minimum);
            for(unsigned i=0;i<8;++i) {
                const glm::dvec3 p=glm::dmat4(draw.World)*glm::dvec4((i&1)?bounds.Max.x:bounds.Min.x,
                    (i&2)?bounds.Max.y:bounds.Min.y,(i&4)?bounds.Max.z:bounds.Min.z,1);
                const glm::dvec3 q(glm::dot(right,p),glm::dot(up,p),glm::dot(light,p));
                minimum=glm::min(minimum,q);maximum=glm::max(maximum,q);
            }
            if(maximum.x<x-radius || minimum.x>x+radius || maximum.y<y-radius || minimum.y>y+radius)continue;
            lo=std::min(lo,minimum.z);hi=std::max(hi,maximum.z);
        }
        lo=std::floor(lo-texel*(1+settings.NormalBias));
        hi=std::ceil(hi+texel*(1+settings.NormalBias));
        const double span=hi-lo;
        glm::dmat4 matrix(1);
        for(unsigned c=0;c<3;++c){matrix[c][0]=right[c]/radius;matrix[c][1]=up[c]/radius;matrix[c][2]=-2*light[c]/span;}
        matrix[3]={-x/radius,-y/radius,(hi+lo)/span,1};
        result.VP[cascade]=glm::mat4(matrix);
        result.TexelDepth[cascade]={float(texel),float(1/span),0,0};
    }
    result.Eye=glm::vec4(eye,1);result.Forward=glm::vec4(forward,0);
    result.Parameters={float(settings.Resolution),settings.NormalBias,settings.DepthBias,settings.Blend};
    result.Range={float(nearDepth),float(end),1,0};
    return result;
}
}
