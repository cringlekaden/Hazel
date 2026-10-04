// Pixel/readback regression tests for the actual upstream Renderer2D pipelines.
#include "Hazel.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
using namespace Hazel;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static glm::mat4 Transform(float x=0, float y=0, float size=1) {
    return glm::translate(glm::mat4(1),{x,y,0})*glm::scale(glm::mat4(1),{size,size,1});
}
struct Fixture {
    std::filesystem::path Directory=std::filesystem::temp_directory_path()/
        ("hazel-renderer2d-"+std::to_string(std::random_device{}()));
    Fixture() { Check(std::filesystem::create_directory(Directory),"Fixture isolation failed"); }
    ~Fixture() { std::error_code e; std::filesystem::remove_all(Directory,e); }
};
static Ref<Framebuffer> target;
static void Begin() {
    target->Bind(); glDisable(GL_DEPTH_TEST);
    RenderCommand::SetClearColor({0,0,0,0}); RenderCommand::Clear();
    target->ClearAttachment(1,-1); Renderer2D::ResetStats();
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));
}
static void End() { Renderer2D::EndScene(); Check(glGetError()==GL_NO_ERROR,"Renderer2D OpenGL error"); }
static int ID(int x=64,int y=64) { return target->ReadPixel(1,x,y); }
static std::array<unsigned char,4> Color(int x=64,int y=64) {
    std::array<unsigned char,4> result{}; glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.data()); return result;
}
static std::size_t Pixels(int entity) {
    std::vector<int> ids(128*128); glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0,0,128,128,GL_RED_INTEGER,GL_INT,ids.data()); glReadBuffer(GL_COLOR_ATTACHMENT0);
    return std::count(ids.begin(),ids.end(),entity);
}
static void Primitives() {
    Begin(); Renderer2D::DrawQuad(Transform(),{1,0,0,1},42); End();
    Check(ID()==42 && Color()[0]>250 && Color()[1]<5,"Solid quad color/entity mismatch");
    Begin(); Renderer2D::DrawRotatedQuad(glm::vec3(0),{1,1},45,{0,1,0,1}); End();
    Check(Color(96,64)[1]>250 && Color(96,96)[1]<5,"Rotated quad footprint mismatch");
    Begin(); Renderer2D::DrawCircle(Transform(),{0,0,1,1},1,.005f,77); End();
    Check(ID()==77 && ID(90,90)==-1 && Color()[2]>250,"Circle picking/discard mismatch");
    Begin(); Renderer2D::DrawCircle(Transform(),{1,1,1,1},.15f,.005f,78); End();
    Check(ID()==-1 && ID(94,64)==78,"Circle ring thickness mismatch");
    Renderer2D::SetLineWidth(2);
    Begin(); Renderer2D::DrawLine({-.5f,0,0},{.5f,0,0},{1,1,0,1},79); End();
    Check(Pixels(79)>60,"Line color/entity output missing");
    Begin(); Renderer2D::DrawRect(Transform(),{1,1,1,1},80); End();
    Check(ID()==-1 && Pixels(80)>120,"Transformed rectangle footprint mismatch");
    Begin(); Renderer2D::DrawRect({0,0,0},{1,1},{1,1,1,1},81); End();
    Check(ID()==-1 && Pixels(81)>120,"Position rectangle footprint mismatch");
}
static void Textures() {
    TextureSpecification specification; specification.Width=2; specification.Height=2; specification.GenerateMips=false; specification.MinFilter=TextureFilter::Linear;
    auto texture=Texture2D::Create(specification);
    const std::array<unsigned char,16> texels{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    texture->SetData(texels.data(),texels.size());
    SpriteRendererComponent sprite; sprite.SetTexture(texture,2); sprite.Color={.5f,1,1,1};
    Begin(); Renderer2D::DrawSprite(Transform(),sprite,43); End();
    Check(ID(40,40)==43 && Color(40,40)[0]>=126 && Color(40,40)[0]<=129 &&
          Color(56,40)[1]>250 && Color(72,40)[0]>=126,"Sprite texture/tiling/tint mismatch");
    sprite.SetTexture(nullptr); sprite.Color={0,1,0,1};
    Begin(); Renderer2D::DrawSprite(Transform(),sprite,44); End();
    Check(ID()==44 && Color()[1]>250,"Untextured sprite mismatch");
    TextureSpecification single; single.GenerateMips=false; single.MinFilter=TextureFilter::Linear; auto white=Texture2D::Create(single);
    const unsigned whitePixel=0xffffffff; white->SetData(&whitePixel,sizeof(whitePixel));
    for(int variant=0;variant<4;++variant) {
        Begin();
        if(variant==0) Renderer2D::DrawQuad(glm::vec2(0),{1,1},white,2,{0,1,0,1});
        if(variant==1) Renderer2D::DrawQuad(glm::vec3(0),{1,1},white,2,{0,1,0,1});
        if(variant==2) Renderer2D::DrawRotatedQuad(glm::vec2(0),{1,1},45,white,2,{0,1,0,1});
        if(variant==3) Renderer2D::DrawRotatedQuad(glm::vec3(0),{1,1},45,white,2,{0,1,0,1});
        End(); Check(Color()[1]>250 && Color()[0]<5 && ID()==-1,"Position/rotated textured quad overload failed tint or default entity ID");
        if(variant>=2) Check(Color(96,64)[1]>250 && Color(96,96)[1]<5,"Rotated textured quad lost rotation");
        const auto stats=Renderer2D::GetStats();
        Check(stats.DrawCalls==1 && stats.QuadCount==1 && stats.GetTotalVertexCount()==4 && stats.GetTotalIndexCount()==6,"Published renderer statistics helpers changed");
    }
    const unsigned limit=std::min(32u,RenderCommand::GetMaxTextureSlots());
    std::vector<Ref<Texture2D>> textures;
    Begin();
    for (unsigned i=0;i<limit+1;++i) {
        TextureSpecification one; one.GenerateMips=false; one.MinFilter=TextureFilter::Linear;
        auto current=Texture2D::Create(one); const unsigned data=0xff00ff00;
        current->SetData(&data,sizeof(data)); textures.push_back(current);
        Renderer2D::DrawQuad(Transform(),current,1,glm::vec4(1),100+i);
    }
    End();
    // White reserves one slot: with a two-slot setting each unique texture
    // occupies its own batch. Larger tested capacities require two batches.
    const auto expectedBatches=(limit+1+(limit-2))/(limit-1);
    Check(Renderer2D::GetStats().DrawCalls==expectedBatches,"Texture slot overflow produced incorrect batch count");
    Check(ID()==int(100+limit),"Texture slot overflow lost final quad");
    std::cout<<"Selected fragment texture batch capacity: "<<limit<<'\n';
}
static void Text() {
    auto font=Font::GetDefault(); Renderer2D::TextParams parameters;
    Begin(); Renderer2D::DrawString(u8"é",font,Transform(-.3f,-.3f,1),parameters,90); End();
    Check(Renderer2D::GetStats().QuadCount==1 && Pixels(90)>100,"UTF-8 glyph rendering/picking mismatch");
    Begin(); Renderer2D::DrawString("A \tA\nA\r",font,Transform(-.5f,.1f,.7f),{glm::vec4(1),.01f,.02f},91); End();
    Check(Renderer2D::GetStats().QuadCount==3 && Pixels(91)>50,"Text whitespace/spacing rendering mismatch");
    auto bold=CreateRef<Font>("assets/fonts/opensans/OpenSans-Bold.ttf");
    Begin(); Renderer2D::DrawString("A",font,Transform(-.8f,-.3f,.8f),parameters,92);
    Renderer2D::DrawString("A",bold,Transform(.1f,-.3f,.8f),parameters,93); End();
    Check(Renderer2D::GetStats().DrawCalls==2 && Pixels(92)>50 && Pixels(93)>50,"Font atlas switch lost pending text");
    Begin(); Renderer2D::DrawString("",font,glm::mat4(1),parameters); End();
    Check(Renderer2D::GetStats().DrawCalls==0,"Empty text produced draw");
    Begin(); Renderer2D::DrawString("?",font,Transform(-.3f,-.3f,1),parameters,94); End();
    const auto fallbackPixels=Pixels(94);
    Begin(); Renderer2D::DrawString(u8"🙂",font,Transform(-.3f,-.3f,1),parameters,94); End();
    Check(Renderer2D::GetStats().QuadCount==1 && Pixels(94)==fallbackPixels,"Non-Latin1 UTF-8 fallback changed target atlas behavior");
    bool rejected=false;
    try { Renderer2D::DrawString("A",{},glm::mat4(1),parameters); } catch(const std::invalid_argument&) { rejected=true; }
    Check(rejected,"Invalid text font accepted");
}
static void Overflow() {
    for (int kind=0;kind<4;++kind) {
        Begin(); const unsigned capacity=kind==2 ? 40000 : 20000;
        for(unsigned i=0;i<=capacity;++i) {
            const bool final=i==capacity; const auto transform=Transform(final?-.2f:10,final?-.2f:10);
            if(kind==0) Renderer2D::DrawQuad(transform,{1,0,0,1},110);
            if(kind==1) Renderer2D::DrawCircle(transform,{0,1,0,1},1,.005f,111);
            if(kind==2) Renderer2D::DrawLine({final?-.5f:10,0,0},{final?.5f:11,0,0},{0,0,1,1},112);
            if(kind==3) Renderer2D::DrawString("A",Font::GetDefault(),transform,{},113);
        }
        End(); Check(Renderer2D::GetStats().DrawCalls==2 && Pixels(110+kind)>50,"Primitive capacity overflow lost final output");
        if(kind!=2) Check(Renderer2D::GetStats().QuadCount==capacity+1,"Overflow statistics lost geometry");
    }
}
static void GenericSubmit() {
    const std::string vertex=R"(#version 330 core
layout(location=0) in vec3 a_Position;
uniform mat4 u_ViewProjection; uniform mat4 u_Transform;
void main(){gl_Position=u_ViewProjection*u_Transform*vec4(a_Position,1);})";
    const std::string fragment=R"(#version 330 core
layout(location=0) out vec4 color; layout(location=1) out int entity;
void main(){color=vec4(1,0,1,1);entity=84;})";
    auto shader=Shader::Create("GenericSubmit",vertex,fragment); auto vao=VertexArray::Create();
    float vertices[]{-.5f,-.5f,0, .5f,-.5f,0, .5f,.5f,0, -.5f,.5f,0};
    auto buffer=VertexBuffer::Create(vertices,sizeof(vertices)); buffer->SetLayout({{ShaderDataType::Float3,"a_Position"}}); vao->AddVertexBuffer(buffer);
    unsigned indices[]{0,1,2,2,3,0}; vao->SetIndexBuffer(IndexBuffer::Create(indices,6));
    Begin(); Renderer2D::EndScene(); OrthographicCamera camera(-1,1,-1,1);
    Renderer::BeginScene(camera); Renderer::Submit(shader,vao,Transform(.5f,0,.5f)); Renderer::EndScene();
    Check(ID(96,64)==84 && ID()==-1 && Color(96,64)[2]>250,"Generic shader submit transform mismatch");
}
int main() {
    try {
        Log::Init(); Fixture fixture; const auto cwd=std::filesystem::current_path();
        ApplicationSpecification invalid; invalid.Name="Migration Missing Shader"; invalid.Resources.Root=fixture.Directory/"missing-resources";
        bool rejected=false; try { Application failed(invalid); } catch(const std::exception&) { rejected=true; }
        Check(rejected && std::filesystem::current_path()==cwd,"Failed renderer startup did not recover working directory");
        for (bool reduced : {false, true}) {
            ApplicationSpecification specification; specification.Name="Migration Renderer2D";
            if (reduced) { specification.Rendering.TextureSlots=2; specification.Rendering.PreferShaderBinaries=false; }
            Application application(specification);
            struct TargetCleanup { ~TargetCleanup() { target.reset(); } } targetCleanup;
            glfwHideWindow(static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow()));
            std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<"; Version: "<<glGetString(GL_VERSION)<<'\n';
            FramebufferSpecification framebuffer; framebuffer.Width=128; framebuffer.Height=128;
            framebuffer.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
            target=Framebuffer::Create(framebuffer);
            Check(Renderer::GetSettings().TextureSlots==(reduced ? 2u : std::min(32u,Renderer::GetCapabilities().MaxTextureSlots)),"Application renderer settings mismatch");
            Primitives(); Textures(); Text(); Overflow(); GenericSubmit();
            Check(glGetError()==GL_NO_ERROR,"Final renderer GL error");
            target->Unbind(); target.reset(); glEnable(GL_DEPTH_TEST);
        }
        std::cout<<"PASS: default and two-slot/GLSL settings, actual quad/circle/line/text shaders, color/entity picking, rotation/tiling/tint/sprites, UTF-8/atlas switch, all primitive capacity flushes, generic submit and failed-constructor recovery\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; target.reset(); return 1; }
}
