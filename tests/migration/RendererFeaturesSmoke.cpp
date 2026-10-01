// Real-context verification of target shader/texture/framebuffer features on 4.1/4.2.
#include "Hazel/Core/Log.h"
#include "Hazel/Renderer/Framebuffer.h"
#include "Hazel/Renderer/Buffer.h"
#include "Hazel/Renderer/VertexArray.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Renderer/UniformBuffer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <random>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

struct FixtureDirectory {
    std::filesystem::path Previous=std::filesystem::current_path(),Path;
    FixtureDirectory() {
        std::random_device random;
        Path=std::filesystem::temp_directory_path()/("hazel-renderer-features-"+std::to_string(random())+"-"+std::to_string(random()));
        if (!std::filesystem::create_directory(Path)) throw std::runtime_error("Could not create isolated test directory");
        std::filesystem::current_path(Path);
    }
    ~FixtureDirectory() {
        std::error_code error;
        std::filesystem::current_path(Previous,error);
        std::filesystem::remove_all(Path,error); // Only this test's uniquely created directory.
    }
};
static void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
static void NoErrors(const char* operation) {
    const GLenum error=glGetError();
    if (error!=GL_NO_ERROR) throw std::runtime_error(std::string(operation)+": OpenGL error "+std::to_string(error));
}
template<class Function> static void MustThrow(Function function,const char* message) {
    bool caught=false; try { function(); } catch (const std::exception&) { caught=true; } Check(caught,message);
}
static std::array<unsigned char,4> ReadColor(const Hazel::Ref<Hazel::Framebuffer>& framebuffer)
{
    framebuffer->GetColorAttachmentRendererID(); // MSAA resolve, if needed.
    GLuint temporary=0; glGenFramebuffers(1,&temporary);
    GLint previous=0; glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previous);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,temporary);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,framebuffer->GetColorAttachmentRendererID(),0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    std::array<unsigned char,4> result{}; glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previous); glDeleteFramebuffers(1,&temporary);
    return result;
}
static std::size_t CountCache() {
    std::size_t count=0; for (const auto& entry:std::filesystem::directory_iterator("assets/cache/shader/opengl"))
        if (entry.is_regular_file()) ++count;
    return count;
}
static void WriteShader(const std::filesystem::path& path,float red)
{
    std::ofstream file(path);
    file<<R"(#type vertex
#version 450 core
layout(location=0) in vec2 position;
layout(std140,binding=3) uniform Camera { mat4 viewProjection; };
layout(location=0) out vec2 uv;
void main() {
 uv=position*0.5+0.5;
 gl_Position=viewProjection*vec4(position,0,1);
}
#type fragment
#version 450 core
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(location=1) out int entity;
layout(binding=5) uniform sampler2D image;
void main() { color=texture(image,uv)*vec4()"<<red<<R"(,1,1,1); entity=73; }
)";
    Check(static_cast<bool>(file),"Could not write shader fixture");
}
static void CheckTextures()
{
    using namespace Hazel;
    GLuint sentinel=0,pbo=0; glGenTextures(1,&sentinel); glBindTexture(GL_TEXTURE_2D,sentinel);
    glGenBuffers(1,&pbo); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo); glBufferData(GL_PIXEL_UNPACK_BUFFER,64,nullptr,GL_STREAM_DRAW);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8); glPixelStorei(GL_UNPACK_ROW_LENGTH,9);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,1); glPixelStorei(GL_UNPACK_SKIP_PIXELS,2);
    for (auto format:{ImageFormat::R8,ImageFormat::RGB8,ImageFormat::RGBA8,ImageFormat::RGBA32F}) {
        TextureSpecification specification; specification.Width=3; specification.Height=2;
        specification.Format=format; specification.GenerateMips=true;
        auto texture=Texture2D::Create(specification);
        Check(texture->IsLoaded() && texture->GetSpecification().Format==format,"Texture specification/load state lost");
        const uint32_t channels=format==ImageFormat::R8 ? 1u : format==ImageFormat::RGB8 ? 3u : 4u;
        const bool floating=format==ImageFormat::RGBA32F;
        std::vector<unsigned char> bytes(6*channels,173);
        std::vector<float> floats(6*channels,0.625f);
        texture->SetData(floating ? static_cast<const void*>(floats.data()) : static_cast<const void*>(bytes.data()),
                         floating ? static_cast<uint32_t>(floats.size()*4) : static_cast<uint32_t>(bytes.size()));
        GLint state=0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&state); Check(state==static_cast<GLint>(sentinel),"Texture upload changed binding");
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&state); Check(state==static_cast<GLint>(pbo),"Upload changed unpack buffer");
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&state); Check(state==8,"Upload changed alignment");
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&state); Check(state==9,"Upload changed row length");
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&state); Check(state==1,"Upload changed skip rows");
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&state); Check(state==2,"Upload changed skip pixels");
        texture->Bind();
        GLenum dataFormat=channels==1 ? GL_RED : channels==3 ? GL_RGB : GL_RGBA;
        glPixelStorei(GL_PACK_ALIGNMENT,1);
        if (floating) {
            std::vector<float> readback(floats.size()); glGetTexImage(GL_TEXTURE_2D,0,dataFormat,GL_FLOAT,readback.data());
            for (auto value:readback) Check(std::abs(value-0.625f)<0.0001f,"Floating texture upload differs");
        } else {
            std::vector<unsigned char> readback(bytes.size()); glGetTexImage(GL_TEXTURE_2D,0,dataFormat,GL_UNSIGNED_BYTE,readback.data());
            Check(readback==bytes,"Byte texture/alignment upload differs");
        }
        GLint mipWidth=0; glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&mipWidth); Check(mipWidth==1,"Mip generation lost");
        MustThrow([&]{texture->SetData(bytes.data(),1);},"Partial texture upload accepted");
        MustThrow([&]{texture->SetData(nullptr,1);},"Null texture upload accepted");
        glBindTexture(GL_TEXTURE_2D,sentinel);
        NoErrors("Texture format/alignment/mips");
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glDeleteBuffers(1,&pbo);
    glBindTexture(GL_TEXTURE_2D,0); glDeleteTextures(1,&sentinel);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_PACK_ALIGNMENT,4);
    TextureSpecification invalid; invalid.Width=0;
    MustThrow([&]{Texture2D::Create(invalid);},"Zero texture dimensions accepted");
    MustThrow([]{Texture2D::Create("missing-texture.png");},"Missing texture accepted");
    // Load a tiny RGB image with a non-ASCII filename through the portable filesystem.
    const auto path=std::filesystem::u8path(u8"texture-\u00e9.ppm");
    { std::ofstream image(path,std::ios::binary); image<<"P6\n3 1\n255\n"; const char pixels[9]={1,2,3,4,5,6,7,8,9}; image.write(pixels,9); }
    auto loaded=Texture2D::Create(path.u8string());
    Check(loaded->GetWidth()==3 && loaded->GetHeight()==1 && loaded->GetSpecification().Format==ImageFormat::RGB8,"UTF-8 image load/specification failed");
    loaded->Bind(); std::array<unsigned char,9> pixel{}; glPixelStorei(GL_PACK_ALIGNMENT,1);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,pixel.data()); Check(pixel[0]==1 && pixel[8]==9,"Loaded RGB alignment lost");
    glPixelStorei(GL_PACK_ALIGNMENT,4); std::filesystem::remove(path);
    NoErrors("Image loading");
}
static void CheckFramebuffersAndShaders()
{
    using namespace Hazel;
    const auto source=std::filesystem::u8path(u8"pipeline-\u00e9.glsl"); WriteShader(source,1.0f);
    std::size_t initial=0;
    if (std::filesystem::exists("assets/cache/shader/opengl")) initial=CountCache();
    auto shader=Shader::Create(source.u8string()); const auto cold=CountCache();
    Check(cold>=4 && cold>=initial,"SPIR-V cache files absent");
    shader.reset(); shader=Shader::Create(source.u8string());
    Check(CountCache()==cold,"Warm cache created duplicate files");
    auto camera=UniformBuffer::Create(64,3);
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; camera->SetData(identity.data(),64);
    TextureSpecification textureSpecification; textureSpecification.GenerateMips=false;
    auto texture=Texture2D::Create(textureSpecification); const std::array<unsigned char,4> color={128,64,192,255}; texture->SetData(color.data(),4);
    auto vao=VertexArray::Create();
    const std::array<float,6> triangle={-1,-1,3,-1,-1,3};
    auto vertices=VertexBuffer::Create(sizeof(triangle)); vertices->SetData(triangle.data(),sizeof(triangle));
    vertices->SetLayout({{ShaderDataType::Float2,"position"}}); vao->AddVertexBuffer(vertices); vao->Bind();
    FramebufferSpecification specification; specification.Width=16; specification.Height=16;
    specification.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH24STENCIL8};
    GLint integerSamples=0; glGetIntegerv(GL_MAX_INTEGER_SAMPLES,&integerSamples);
    std::vector<uint32_t> pickingSamples={1};
    if (integerSamples>=4) pickingSamples.push_back(4);
    else {
        auto unsupported=specification; unsupported.Samples=4;
        MustThrow([&]{Framebuffer::Create(unsupported);},"Unsupported integer MSAA silently accepted/downgraded");
        std::cout<<"Hardware integer MSAA unavailable; verified explicit rejection and retained single-sample picking\n";
    }
    for (uint32_t samples:pickingSamples) {
        specification.Samples=samples;
        auto framebuffer=Framebuffer::Create(specification);
        framebuffer->Bind(); framebuffer->ClearAttachment(0,0); framebuffer->ClearAttachment(1,-1);
        glDisable(GL_DEPTH_TEST); shader->Bind(); texture->Bind(5); glDrawArrays(GL_TRIANGLES,0,3);
        Check(framebuffer->ReadPixel(1,8,8)==73,"Integer picking/readback or reflected UBO binding failed");
        auto readback=ReadColor(framebuffer);
        std::cout<<"Readback samples="<<samples<<": "<<int(readback[0])<<','<<int(readback[1])<<','<<int(readback[2])<<','<<int(readback[3])<<"; entity=73\n";
        Check(std::abs(static_cast<int>(readback[0])-128)<=1 && std::abs(static_cast<int>(readback[1])-64)<=1 &&
              std::abs(static_cast<int>(readback[2])-192)<=1 && readback[3]==255,"Color readback or reflected sampler binding failed");
        // A farther draw must not overwrite the near color through the depth attachment.
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
        auto nearMatrix=identity; nearMatrix[14]=-0.5f; camera->SetData(nearMatrix.data(),64); glDrawArrays(GL_TRIANGLES,0,3);
        auto farMatrix=identity; farMatrix[14]=0.5f; camera->SetData(farMatrix.data(),64);
        const std::array<unsigned char,4> fartherColor={0,255,0,255}; texture->SetData(fartherColor.data(),4); glDrawArrays(GL_TRIANGLES,0,3);
        auto depthResult=ReadColor(framebuffer);
        Check(std::abs(static_cast<int>(depthResult[0])-128)<=1 && std::abs(static_cast<int>(depthResult[1])-64)<=1,"Depth occlusion failed");
        glDisable(GL_DEPTH_TEST); camera->SetData(identity.data(),64); texture->SetData(color.data(),4);
        // Clear must affect the entire attachment even under caller masks/scissor.
        glEnable(GL_SCISSOR_TEST); glScissor(0,0,1,1); glColorMaski(0,GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        framebuffer->ClearAttachment(1,-17);
        Check(framebuffer->ReadPixel(1,8,8)==-17,"Full integer clear was restricted by caller state");
        Check(glIsEnabled(GL_SCISSOR_TEST),"Clear/resolve changed scissor state");
        GLboolean mask[4]; glGetBooleani_v(GL_COLOR_WRITEMASK,0,mask); Check(!mask[0] && !mask[3],"Clear changed write mask");
        glDisable(GL_SCISSOR_TEST); glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        framebuffer->Resize(32,24); Check(framebuffer->GetSpecification().Width==32,"Framebuffer resize lost specification");
        framebuffer->ClearAttachment(1,101); Check(framebuffer->ReadPixel(1,31,23)==101,"Resized picking attachment failed");
        framebuffer->Resize(0,0); Check(framebuffer->GetSpecification().Width==32,"Zero viewport resize damaged framebuffer");
        MustThrow([&]{framebuffer->ReadPixel(0,1,1);},"Color attachment accepted integer read");
        MustThrow([&]{framebuffer->ReadPixel(1,-1,0);},"Out-of-range pixel accepted");
        MustThrow([&]{framebuffer->ClearAttachment(9,1);},"Invalid attachment accepted");
        framebuffer->Unbind(); NoErrors("Framebuffer/picking/clear/resize/MSAA");
    }
    // Color/depth MSAA remains available when integer multisample storage is unsupported.
    FramebufferSpecification colorMSAA; colorMSAA.Width=16; colorMSAA.Height=16; colorMSAA.Samples=4;
    colorMSAA.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::Depth};
    auto multisampleColor=Framebuffer::Create(colorMSAA); multisampleColor->Bind();
    multisampleColor->ClearAttachment(0,0); shader->Bind(); texture->Bind(5); glDrawArrays(GL_TRIANGLES,0,3);
    auto resolvedColor=ReadColor(multisampleColor);
    Check(std::abs(static_cast<int>(resolvedColor[0])-128)<=1 && std::abs(static_cast<int>(resolvedColor[2])-192)<=1,
          "Color/depth MSAA resolve failed");
    multisampleColor->Unbind(); NoErrors("Color/depth MSAA");
    std::cout<<"PASS: four-sample color/depth framebuffer and resolved color readback\n";
    FramebufferSpecification depth; depth.Width=8; depth.Height=8; depth.Attachments={FramebufferTextureFormat::Depth};
    auto depthOnly=Framebuffer::Create(depth); depthOnly->Bind(); Check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Depth-only framebuffer incomplete"); depthOnly->Unbind();
    // Same path, modified content must generate a distinct fragment cache key.
    WriteShader(source,0.5f); auto changed=Shader::Create(source.u8string());
    Check(CountCache()>cold,"Modified source reused stale filename-only cache");
    // Reject and regenerate a structurally damaged cache without disabling caching.
    for (const auto& entry:std::filesystem::directory_iterator("assets/cache/shader/opengl")) {
        std::ofstream damaged(entry.path(),std::ios::binary|std::ios::trunc); damaged<<"broken";
    }
    shader.reset(); shader=Shader::Create(source.u8string());
    shader->Bind(); texture->Bind(5);
    specification.Samples=1;
    auto framebuffer=Framebuffer::Create(specification); framebuffer->Bind(); glDrawArrays(GL_TRIANGLES,0,3);
    auto half=ReadColor(framebuffer); Check(std::abs(static_cast<int>(half[0])-64)<=1,"Content change/corrupt cache recovery rendered stale shader"); framebuffer->Unbind();
    MustThrow([] { Shader::Create("bad","#version 450\ninvalid","#version 450\ninvalid"); },"Invalid shader compiled without an exception");
    std::filesystem::remove(source); vao->Unbind();
    NoErrors("Cold/warm/changed/corrupt shader caches and depth-only framebuffer");
}
int main()
{
    FixtureDirectory fixtures;
    Hazel::Log::Init();
    glfwSetErrorCallback([](int code,const char* message){std::cerr<<"GLFW "<<code<<": "<<message<<'\n';});
    if (!glfwInit()) { std::cerr<<"FAIL: GLFW initialization\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* window=glfwCreateWindow(64,64,"Migration renderer features",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    int result=0;
    try {
        glfwMakeContextCurrent(window); Check(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))!=0,"GLAD failed");
        std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<"; Context: "<<glGetString(GL_VERSION)<<"; GLSL "<<glGetString(GL_SHADING_LANGUAGE_VERSION)<<'\n';
        GLint maxSamples=0,maxIntegerSamples=0,maxColorSamples=0,maxDepthSamples=0;
        glGetIntegerv(GL_MAX_SAMPLES,&maxSamples); glGetIntegerv(GL_MAX_INTEGER_SAMPLES,&maxIntegerSamples);
        glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,&maxColorSamples); glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES,&maxDepthSamples);
        std::cout<<"Sample limits: renderbuffer="<<maxSamples<<", integer texture="<<maxIntegerSamples
                 <<", color texture="<<maxColorSamples<<", depth texture="<<maxDepthSamples<<'\n';
        CheckTextures(); CheckFramebuffersAndShaders();
        std::cout<<"PASS: real-context texture formats/mips/UTF-8/alignment/state, reflected shader bindings, cold/warm/changed/corrupt caches, framebuffer color/entity readback, full clear, resize, depth-only and MSAA resolve\n";
    } catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; result=1; }
    glfwDestroyWindow(window); glfwTerminate(); return result;
}
