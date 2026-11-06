// Direct_v1 GPU contract. FP64 oracle below is independent of shader helpers.
#include "Hazel/Renderer/SceneRenderer.h"
#include "Hazel/Assets/NativeStarter.h"
#include "Hazel/Assets/ProjectAssets.h"
#include <cmath>
static glm::dvec3 OracleBRDF(glm::dvec3 base,double metal,double rough,glm::dvec3 n,glm::dvec3 v,glm::dvec3 l){
    const double pi=std::acos(-1.0);double nl=glm::dot(n,l),nv=glm::dot(n,v);if(nl<=0||nv<=0)return {};
    auto h=glm::normalize(v+l);double nh=glm::dot(n,h),vh=glm::dot(v,h),alpha=std::max(.045,rough);alpha*=alpha;double alpha2=alpha*alpha;
    double d=alpha2/(pi*std::pow(1+(alpha2-1)*nh*nh,2));
    double lambdaV=std::sqrt(1+alpha2*(1/(nv*nv)-1)),lambdaL=std::sqrt(1+alpha2*(1/(nl*nl)-1));
    double visibility=1/(2*nl*nv*(lambdaV+lambdaL));
    auto f0=glm::mix(glm::dvec3(.04),base,metal);auto f=f0+(1.0-f0)*std::pow(1-vh,5);
    return ((1-metal)*(1.0-f)*base/pi+d*visibility*f)*nl;
}
static glm::dvec3 OracleNeutral(glm::dvec3 value){
    double minimum=std::min({value.r,value.g,value.b});double offset=minimum<.08?minimum-6.25*minimum*minimum:.04;value-=offset;
    double peak=std::max({value.r,value.g,value.b});if(peak<.76)return value;
    double compressed=1-.24*.24/(peak+.24-.76);auto scaled=value*(compressed/peak);double amount=1-1/(.15*(peak-compressed)+1);return glm::mix(scaled,glm::dvec3(compressed),amount);
}
static void NearRGB(glm::vec3 actual,glm::dvec3 expected,double tolerance,const char* message){
    for(int c=0;c<3;++c)if(!std::isfinite(actual[c])||std::abs(double(actual[c])-expected[c])>tolerance){std::cerr<<message<<" channel "<<c<<" actual="<<actual[c]<<" expected="<<expected[c]<<'\n';throw std::runtime_error(message);}
}
static void DirectPBRChecks(){
    Fixture fixture;NativeStarter::Generate(fixture.Directory);auto assets=CreateRef<ProjectAssets>(fixture.Directory);
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");Check(bool(mesh.Accepted),"Native cooked starter did not decode");
    FramebufferSpecification spec;spec.Width=spec.Height=128;spec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};auto display=Framebuffer::Create(spec);SceneRenderView view;
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;frame.Presentation.Background={0,0,0,1};frame.Camera.Orthographic=true;frame.Camera.Position={0,0,3};frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0));
    auto light=glm::normalize(glm::vec3(.3f,.4f,1));frame.Light=SubmittedDirectional{light,glm::vec3(2)};
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.18f,.4f,.7f,1};
    SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.World=glm::mat4(1);draw.Picking=123;draw.Materials={material};frame.Meshes={draw};
    auto render=[&](const std::function<void()>& ui=[]{}){display->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();display->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,display,[]{},ui);Check(glGetError()==GL_NO_ERROR,"Direct PBR GL error");};
    for(float metallic:{0.f,.5f,1.f})for(float roughness:{0.f,.1f,.25f,.5f,.75f,1.f}){
        material->Metallic=metallic;material->Roughness=roughness;render();auto expected=OracleBRDF(glm::dvec3(material->BaseColor),metallic,roughness,{0,0,1},{0,0,1},glm::dvec3(light))*2.;
        NearRGB(view.Radiance->ReadColorPixel(0,64,64),expected,.002,"GGX direct FP64 oracle");NearRGB(display->ReadColorPixel(0,64,64),OracleNeutral(expected),.001,"Neutral oracle");Check(display->ReadPixel(1,64,64)==123,"Presented mesh integer picking lost");
    }
    material->Metallic=1;material->Roughness=0;frame.Light=SubmittedDirectional{{0,0,1},glm::vec3(1000)};render();auto highlight=view.Radiance->ReadColorPixel(0,64,64);Check(highlight.b>65504&&std::isfinite(highlight.b),"Bright floor-roughness highlight requires finite FP32 radiance");
    frame.Light=SubmittedDirectional{light,glm::vec3(2)};material->Metallic=0;material->Roughness=.5f;render();auto radiance=view.Radiance->ReadColorPixel(0,64,64);frame.Light->Incident*=2;render();NearRGB(view.Radiance->ReadColorPixel(0,64,64),glm::dvec3(radiance)*2.,.002,"Incident light scaling");
    frame.Light.reset();render();NearRGB(view.Radiance->ReadColorPixel(0,64,64),{},1e-7,"No light must not synthesize ambient");
    material->Model=MaterialModel::Unlit;material->BaseColor={.18f,.4f,.7f,1};
    for(float ev:{-2.f,0.f,2.f}){frame.Presentation.Exposure=ev;render();NearRGB(display->ReadColorPixel(0,64,64),OracleNeutral(glm::dvec3(material->BaseColor)*double(std::exp2(ev))),.001,"Unlit world exposure/tone");}
    frame.Presentation.Exposure=0;render();auto covered=display->ReadColorPixel(0,64,64);material->Alpha=MaterialAlpha::Mask;material->BaseColor.a=.1f;render();Check(display->ReadPixel(1,64,64)==-1,"Masked fragments wrote depth/picking");material->BaseColor.a=1;render();Check(display->ReadPixel(1,64,64)==123,"Mask accepted coverage missing");
    for(auto scale:{glm::vec3(-1,1,1),glm::vec3(1,.5f,2)}){frame.Meshes[0].World=glm::scale(glm::mat4(1),scale);render();Check(display->ReadPixel(1,64,64)==123,"Negative/nonuniform transform winding missing");NearRGB(display->ReadColorPixel(0,64,64),glm::dvec3(covered),.001,"Transformed unlit color changed");}
    frame.Meshes[0].World=glm::mat4(1);
    // A data normal texel is decoded as UNORM, never sRGB. Top-left UV has -Y bitangent on the cube front.
    std::string tga(18,'\0');tga[2]=2;tga[12]=tga[14]=1;tga[16]=32;tga[17]=0x28;tga+=char(255);tga+=char(255);tga+=char(128);tga+=char(255);
    FileSystem::WriteNewFile(fixture.Directory/"Normal.tga",tga);
    material->Model=MaterialModel::MetallicRoughness;material->Textures[2].Image="Normal.tga";frame.Presentation.Exposure=0;frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(.2f,-.4f,1)),glm::vec3(2)};
    for(float mirror:{1.f,-1.f}){
        frame.Meshes[0].World=glm::scale(glm::mat4(1),{mirror,.5f,2});render();auto n=glm::normalize(glm::dvec3(mirror*(2.*128/255-1),-1,1));auto expected=OracleBRDF(glm::dvec3(material->BaseColor),0,.5,n,{0,0,1},glm::dvec3(frame.Light->ToLight))*2.;NearRGB(view.Radiance->ReadColorPixel(0,64,64),expected,.002,"Data normal TBN / mirrored handedness");
    }
    material->Textures[2].Image.clear();frame.Meshes[0].World=glm::mat4(1);
    // A single reversed plane exposes double-sided back normal reversal; a closed cube cannot.
    auto plane=CreateRef<MeshAsset>(*mesh.Accepted);plane->Definition.ID=plane->Geometry.Mesh=30;plane->Generation=31;plane->Geometry.Vertices.resize(4);plane->Geometry.Indices.resize(6);plane->Geometry.Primitives[0].IndexCount=6;plane->Geometry.UpdateBounds();frame.Meshes[0].Geometry=plane;frame.Meshes[0].World=glm::rotate(glm::mat4(1),float(std::acos(-1.)),glm::vec3(0,1,0));material->DoubleSided=false;render();Check(display->ReadPixel(1,64,64)==-1,"Single-sided backface was drawn");material->DoubleSided=true;render();Check(display->ReadPixel(1,64,64)==123,"Double-sided backface missing");NearRGB(view.Radiance->ReadColorPixel(0,64,64),OracleBRDF(glm::dvec3(material->BaseColor),0,.5,{0,0,1},{0,0,1},glm::dvec3(frame.Light->ToLight))*2.,.002,"Double-sided normal reversal");
    frame.Meshes[0].Geometry=mesh.Accepted;frame.Meshes[0].World=glm::mat4(1);material->DoubleSided=false;material->Model=MaterialModel::Unlit;
    // UI runs after presentation, with independent linear color and entity IDs.
    auto ui=[&]{auto state=RenderCommand::GetRasterState();state.DepthTest=false;state.Blend=true;RenderCommand::SetRasterState(state);Renderer2D::BeginScene(glm::mat4(1));Renderer2D::DrawQuad(Transform(),{.25f,.5f,.75f,1},456);Renderer2D::EndScene();};
    for(float ev:{-8.f,8.f}){frame.Presentation.Exposure=ev;render(ui);NearRGB(display->ReadColorPixel(0,64,64),{.25,.5,.75},.001,"UI brightness coupled to world exposure");Check(display->ReadPixel(1,64,64)==456,"UI integer picking missing after presentation");}
    // A hostile caller state must survive successful and failing submissions.
    glEnable(GL_SCISSOR_TEST);glScissor(5,7,11,13);glDepthMask(GL_FALSE);glDepthFunc(GL_GEQUAL);glFrontFace(GL_CW);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);glActiveTexture(GL_TEXTURE4);GLint beforeProgram;glGetIntegerv(GL_CURRENT_PROGRAM,&beforeProgram);
    auto checkState=[&]{GLint value;glGetIntegerv(GL_DEPTH_FUNC,&value);Check(value==GL_GEQUAL&&!RenderCommand::GetRasterState().DepthWrite,"Depth state not restored");glGetIntegerv(GL_ACTIVE_TEXTURE,&value);Check(value==GL_TEXTURE4,"Texture unit not restored");glGetIntegerv(GL_CURRENT_PROGRAM,&value);Check(value==beforeProgram,"Program not restored");Check(RenderCommand::GetRasterState().ClipRect==glm::ivec4(5,7,11,13),"Scissor not restored");};
    SceneRenderer::Render(frame,view,display,[]{},[]{});checkState();bool failed=false;try{SceneRenderer::Render(frame,view,{},[]{},[]{});}catch(const std::exception&){failed=true;}Check(failed,"HDR missing host target silently accepted");checkState();
    glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glFrontFace(GL_CCW);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);display->Unbind();
    for(auto v:{glm::dvec3(0,0,1),glm::normalize(glm::dvec3(.8,0,.2))}){auto l=glm::normalize(glm::dvec3(.2,.3,1));auto a=OracleBRDF({.2,.4,.7},.5,.5,{0,0,1},v,l)/l.z;auto b=OracleBRDF({.2,.4,.7},.5,.5,{0,0,1},l,v)/v.z;Check(glm::length(a-b)<1e-10,"Independent BRDF reciprocity");}
    std::cout<<"PASS: Direct_v1 FP64/GPU roughness-metal grid, finite FP32 highlights, light scaling/no ambient, Neutral/exposure/UI independence, masks, transforms, picking and exception state restoration\n";
}
