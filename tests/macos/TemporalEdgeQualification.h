#pragma once
#include "Renderer3DParity.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>

// AO-free native silhouettes against analytic projected-cube pixel coverage.
// This measures edges, including pixels excluded by the older interior tests.
inline void TemporalEdgeQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    using Point=glm::dvec2;
    const bool sloped=std::getenv("HAZEL_TAA_EDGE_SLOPED")!=nullptr;
    const bool fast=std::getenv("HAZEL_TAA_EDGE_FAST")!=nullptr;
    auto cross=[](Point a,Point b){return a.x*b.y-a.y*b.x;};
    auto area=[&](const std::vector<Point>& polygon){double sum=0;for(size_t i=0;i<polygon.size();++i)sum+=cross(polygon[i],polygon[(i+1)%polygon.size()]);return std::abs(sum)*.5;};
    auto coverage=[&](std::vector<Point> polygon,int x,int y){
        for(int side=0;side<4;++side) {
            std::vector<Point> clipped;const int axis=side/2;const double bound=(axis?y:x)+(side%2);const double sign=side%2?-1:1;
            for(size_t i=0;i<polygon.size();++i) {
                Point a=polygon[i],b=polygon[(i+1)%polygon.size()];double da=sign*(a[axis]-bound),db=sign*(b[axis]-bound);
                if(da>=0)clipped.push_back(a);
                if((da>=0)!=(db>=0))clipped.push_back(a+(b-a)*(da/(da-db)));
            }
            polygon=std::move(clipped);
        }
        return polygon.size()<3?0.:std::min(1.,area(polygon));
    };
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    const auto projectRoot=selected.empty()?std::filesystem::absolute("examples/RenderingReference"):selected.parent_path();
    auto project=Project::LoadCandidate(projectRoot/"RenderingReference-Direct-PerspectivePBR.hproj");
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    auto material=CreateRef<MaterialDefinition>();material->Model=MaterialModel::Unlit;material->BaseColor={.72f,.72f,.72f,1};
    SceneSubmission frame;frame.SceneIdentity=555;frame.Assets=assets;frame.Presentation.SceneReferred=true;frame.Presentation.Background={.12f,.12f,.12f,1};
    SubmittedMesh draw;draw.Identity=1;draw.Picking=-2000000003;draw.Geometry=mesh.Accepted;draw.Materials={material};frame.Meshes={draw};
    FramebufferSpecification spec;spec.Width=320;spec.Height=180;spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
    auto target=Framebuffer::Create(spec);
    auto save=[&](const std::string& name,const auto& image){std::ofstream file(output/(name+".bin"),std::ios::binary);for(size_t p=0;p<image->Color.size();++p){file.write(reinterpret_cast<const char*>(&image->Color[p]),16);file.write(reinterpret_cast<const char*>(&image->Entity[p]),4);file.write(reinterpret_cast<const char*>(&image->Depth[p]),4);}if(!file)throw std::runtime_error("Edge capture failed");};
    std::ofstream csv(output/"edges.csv");csv<<"projection,path,frame,jitter_x,jitter_y,phase,samples,off_rmse,on_rmse,off_residual_change,on_residual_change,accepted_fraction,mean_contribution,jitter_raster_mismatches,soft_width_on,soft_width_reference,mean_history_weight\n";
    for(bool ortho:{false,true})for(unsigned path=0;path<4;++path) {
        SceneRenderView plain,temporal,weight,rejection,raw;weight.TemporalDebug=4;rejection.TemporalDebug=3;raw.TemporalDebug=6;
        std::vector<double> previousOff(320*180),previousOn(320*180);std::vector<bool> previousBand(320*180);
        for(unsigned index=0;index<64;++index) {
            const float t=float(index),translation=fast?std::sin(t*.16f):t;frame.Camera.Orthographic=ortho;frame.Camera.Position={path==1?translation*(fast?.24f:.003f):0,path==1?translation*(fast?.1f:.0013f):0,3};
            auto direction=path==2?glm::vec3(std::sin(fast?.12f*std::sin(t*.16f):t*.0012f),.1f*std::sin(fast?.12f*std::sin(t*.16f):t*.0012f),-std::cos(fast?.12f*std::sin(t*.16f):t*.0012f)):glm::vec3(0,0,-1);
            frame.Camera.ToCamera=-glm::normalize(direction);
            auto projection=ortho?glm::ortho(-2.222222f,2.222222f,-1.25f,1.25f,.1f,fast?1000.f:20.f):glm::perspective(glm::radians(45.f),320.f/180.f,fast?.01f:.1f,fast?1000.f:20.f);
            frame.Camera.ViewProjection=projection*glm::lookAt(frame.Camera.Position,frame.Camera.Position+direction,{0,1,0});
            auto orientation=glm::rotate(glm::mat4(1),.37f+(path==3?t*.0011f:0),{0,0,1});
            if(sloped)orientation=orientation*glm::rotate(glm::mat4(1),1.05f,{0,1,0})*glm::rotate(glm::mat4(1),.35f,{1,0,0});
            frame.Meshes[0].SetWorld(glm::translate(glm::mat4(1),{path==3?t*.003f:0,path==3?t*.0013f:0,0})*
                orientation*glm::scale(glm::mat4(1),{2.6f,.63f,.02f}));
            auto render=[&](SceneRenderView& view,bool enabled){frame.Presentation.TemporalAA=enabled;target->Bind();target->Clear(frame.Presentation.Background);SceneRenderer::Render(frame,view,target,[]{},[]{});auto image=capture(target);if(!image||!image->Completed||!image->Error.empty())throw std::runtime_error("Incomplete edge capture");return image;};
            auto off=render(plain,false),on=render(temporal,true),weights=render(weight,true),valid=render(rejection,true);
            auto jittered=path==0?render(raw,true):Ref<const FramebufferReadback>{};
            if(off->Entity!=on->Entity||off->Depth!=on->Depth)throw std::runtime_error("Edge TAA changed exact IDs/depth");
            if(temporal.Temporal.Phase!=index+1 || !temporal.Temporal.Submitted->load())throw std::runtime_error("Edge temporal history failed to advance");
            std::vector<Point> corners;
            const auto bounds=mesh.Accepted->Geometry.Bounds;
            for(unsigned i=0;i<8;++i){auto p=glm::dmat4(frame.Camera.ViewProjection)*glm::dmat4(frame.Meshes[0].World)*glm::dvec4((i&1)?bounds.Max.x:bounds.Min.x,(i&2)?bounds.Max.y:bounds.Min.y,(i&4)?bounds.Max.z:bounds.Min.z,1);corners.push_back((Point(p)/p.w*.5+.5)*Point(320,180));}
            std::sort(corners.begin(),corners.end(),[](Point a,Point b){return a.x==b.x?a.y<b.y:a.x<b.x;});
            std::vector<Point> hull;for(auto p:corners){while(hull.size()>1&&cross(hull.back()-hull[hull.size()-2],p-hull.back())<=0)hull.pop_back();hull.push_back(p);}auto lower=hull.size();
            for(auto it=corners.rbegin()+1;it!=corners.rend();++it){while(hull.size()>lower&&cross(hull.back()-hull[hull.size()-2],*it-hull.back())<=0)hull.pop_back();hull.push_back(*it);}hull.pop_back();
            std::vector<bool> band(320*180);std::vector<double> errOff(320*180),errOn(320*180);double squareOff=0,squareOn=0,changeOff=0,changeOn=0,accepted=0,contribution=0,linearContribution=0;size_t count=0,changes=0,jitterMismatches=0,softOn=0,softReference=0;double perimeter=0;for(size_t i=0;i<hull.size();++i)perimeter+=glm::length(hull[(i+1)%hull.size()]-hull[i]);
            for(unsigned y=2;y<178;++y)for(unsigned x=2;x<318;++x) {
                Point p(x+.5,y+.5);double distance=1e30;
                for(size_t i=0;i<hull.size();++i){auto a=hull[i],v=hull[(i+1)%hull.size()]-a;distance=std::min(distance,glm::length(p-(a+v*std::clamp(glm::dot(p-a,v)/glm::dot(v,v),0.,1.))));}
                size_t pixel=y*320+x;band[pixel]=distance<1.5;double signal=(on->Color[pixel].r-.08)/.6;softOn+=signal>.05&&signal<.95;
                if(jittered) {
                    Point sample=p-Point(temporal.Temporal.Jitter);bool inside=true;double boundary=1e30;
                    for(size_t i=0;i<hull.size();++i){auto edge=hull[(i+1)%hull.size()]-hull[i];double side=cross(edge,sample-hull[i])/glm::length(edge);inside&=side>=0;boundary=std::min(boundary,std::abs(side));}
                    if(boundary>.002 && (jittered->Color[pixel].r>.38f)!=inside)++jitterMismatches;
                }
                if(!band[pixel]&&!previousBand[pixel])continue;
                double reference=coverage(hull,int(x),int(y));softReference+=reference>.05&&reference<.95;
                errOff[pixel]=(off->Color[pixel].r-.08)/.6-reference;errOn[pixel]=(on->Color[pixel].r-.08)/.6-reference;
                if(band[pixel]){++count;squareOff+=errOff[pixel]*errOff[pixel];squareOn+=errOn[pixel]*errOn[pixel];accepted+=valid->Color[pixel].g>.5f;contribution+=weights->Color[pixel].r;double c=weights->Color[pixel].r;linearContribution+=c<.04?std::sqrt(std::max(c,0.)/6.25):c<.76?c+.04:.56+.0576/(1-c);}
                if(index&&previousBand[pixel]&&band[pixel]){++changes;changeOff+=std::pow(errOff[pixel]-previousOff[pixel],2);changeOn+=std::pow(errOn[pixel]-previousOn[pixel],2);}
            }
            csv<<(ortho?"ortho":"perspective")<<','<<path<<','<<index<<','<<temporal.Temporal.Jitter.x<<','<<temporal.Temporal.Jitter.y<<','<<temporal.Temporal.Phase<<','<<count<<','<<std::sqrt(squareOff/count)<<','<<std::sqrt(squareOn/count)<<','<<(changes?std::sqrt(changeOff/changes):0)<<','<<(changes?std::sqrt(changeOn/changes):0)<<','<<accepted/count<<','<<contribution/count<<','<<jitterMismatches<<','<<softOn/perimeter<<','<<softReference/perimeter<<','<<linearContribution/count<<'\n';
            if(jitterMismatches>2)throw std::runtime_error("Native jitter sign/amplitude differs from analytic projected geometry");
            if(index%8==0 || index==63){auto name=std::string(ortho?"ortho":"perspective")+"-edge-"+std::to_string(path)+"-"+std::to_string(index);save(name+"-off",off);save(name+"-on",on);save(name+"-weight",weights);if(jittered)save(name+"-jittered",jittered);}
            previousOff=std::move(errOff);previousOn=std::move(errOn);previousBand=std::move(band);
            if(index==63) {
                frame.Camera.Position.x+=20;
                frame.Camera.ViewProjection=projection*glm::lookAt(frame.Camera.Position,frame.Camera.Position+direction,{0,1,0});
                render(temporal,true);
                if(temporal.Temporal.Phase!=1)throw std::runtime_error("Large camera cut did not reset edge history");
            }
        }
    }
    target.reset();
}
