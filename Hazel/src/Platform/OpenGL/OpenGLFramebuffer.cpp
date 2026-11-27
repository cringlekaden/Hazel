#include "hzpch.h"
// Adapted upstream framebuffer: binding-based 4.1 allocation, clearing and MSAA resolve.
#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Hazel/Renderer/Scene/Bloom.h"
#include "Hazel/Renderer/Scene/CameraMotion.h"
#include "Hazel/Renderer/Scene/DepthOfField.h"
#include "Hazel/Renderer/Scene/ExposureMeter.h"
#include "Hazel/Renderer/Scene/LensEffects.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Resources/Shader.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <stdexcept>

namespace Hazel {
static const uint32_t s_MaxFramebufferSize=8192;
namespace Utils {
static GLenum TextureTarget(bool multisampled) { return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D; }
static void CreateTextures(bool, uint32_t* outID,uint32_t count) {
    glGenTextures(static_cast<GLsizei>(count),outID);
    for (uint32_t i=0;i<count;++i) if (!outID[i]) throw std::runtime_error("Failed to create framebuffer texture");
}
static void BindTexture(bool multisampled,uint32_t id) { glBindTexture(TextureTarget(multisampled),id); }
static bool IsDepthFormat(FramebufferTextureFormat format) { return format==FramebufferTextureFormat::DEPTH24STENCIL8 || format==FramebufferTextureFormat::DEPTH32F; }
struct FramebufferState {
    GLint Draw=0,Read=0;
    FramebufferState() { glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&Draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&Read); }
    ~FramebufferState() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER,Draw); glBindFramebuffer(GL_READ_FRAMEBUFFER,Read); }
};
struct TextureBindingState {
    GLenum Target; GLint Binding=0;
    explicit TextureBindingState(bool multisampled):Target(TextureTarget(multisampled)) {
        glGetIntegerv(multisampled ? GL_TEXTURE_BINDING_2D_MULTISAMPLE : GL_TEXTURE_BINDING_2D,&Binding);
    }
    ~TextureBindingState() { glBindTexture(Target,Binding); }
};
static void AttachColorTexture(uint32_t id,int samples,GLenum internalFormat,GLenum format,
                               uint32_t width,uint32_t height,int index)
{
    const bool multisampled=samples>1;
    BindTexture(multisampled,id);
    if (multisampled) glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,samples,internalFormat,width,height,GL_FALSE);
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
                     format == GL_RED_INTEGER ? GL_INT
                     : (internalFormat == GL_RGBA16F || internalFormat == GL_RGBA32F)
                         ? GL_FLOAT
                         : GL_UNSIGNED_BYTE,
                     nullptr);
        const GLint filter=format==GL_RED_INTEGER ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+index,TextureTarget(multisampled),id,0);
}
static void AttachDepthTexture(uint32_t id,int samples,GLenum format,GLenum attachmentType,uint32_t width,uint32_t height)
{
    const bool multisampled=samples>1;
    BindTexture(multisampled,id);
    if (multisampled) glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,samples,format,width,height,GL_FALSE);
    else {
        // glTexStorage2D is 4.2; equivalent depth/stencil storage retains the 4.1 path.
        glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,format==GL_DEPTH_COMPONENT32F?GL_DEPTH_COMPONENT:GL_DEPTH_STENCIL,
            format==GL_DEPTH_COMPONENT32F?GL_FLOAT:GL_UNSIGNED_INT_24_8,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER,attachmentType,TextureTarget(multisampled),id,0);
}
static void SetDrawBuffers(std::size_t count)
{
    if (!count) { glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); return; }
    std::vector<GLenum> attachments(count);
    for (std::size_t i=0;i<count;++i) attachments[i]=GL_COLOR_ATTACHMENT0+static_cast<GLenum>(i);
    glDrawBuffers(static_cast<GLsizei>(count),attachments.data());
}
}

OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec):m_Specification(spec)
{
    for (auto attachment:spec.Attachments.Attachments) {
        if (Utils::IsDepthFormat(attachment.TextureFormat)) {
            if (m_DepthAttachmentSpecification.TextureFormat!=FramebufferTextureFormat::None)
                throw std::invalid_argument("Framebuffer has multiple depth attachments");
            m_DepthAttachmentSpecification=attachment;
        } else {
            if (attachment.TextureFormat != FramebufferTextureFormat::RGBA32F &&
                attachment.TextureFormat != FramebufferTextureFormat::RGBA16F &&
                attachment.TextureFormat != FramebufferTextureFormat::RGBA8 &&
                attachment.TextureFormat != FramebufferTextureFormat::RED_INTEGER)
                throw std::invalid_argument("Unsupported framebuffer color attachment");
            m_ColorAttachmentSpecifications.emplace_back(attachment);
        }
    }
    Invalidate();
}

void OpenGLFramebuffer::Release()
{
    RestoreBlending();
    glDeleteFramebuffers(1,&m_RendererID);
    glDeleteFramebuffers(1,&m_ResolveRendererID);
    glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()),m_ColorAttachments.data());
    glDeleteTextures(static_cast<GLsizei>(m_ResolveColorAttachments.size()),m_ResolveColorAttachments.data());
    glDeleteTextures(1,&m_DepthAttachment);
    m_RendererID=m_ResolveRendererID=m_DepthAttachment=0;
    m_ColorAttachments.clear(); m_ResolveColorAttachments.clear();
}
OpenGLFramebuffer::~OpenGLFramebuffer()
{
    if (m_BloomQueries.front())
        glDeleteQueries(GLsizei(m_BloomQueries.size()), m_BloomQueries.data());
    Release();
    glDeleteVertexArrays(1, &m_OutputVAO);
}

void OpenGLFramebuffer::Invalidate()
{
    if (m_Specification.SwapChainTarget) return;
    const auto& caps = OpenGLCapabilities::Get();
    const auto maximum = caps.MaxTextureSize;
    const auto drawBuffers = caps.MaxDrawBuffers, colorAttachments = caps.MaxColorAttachments;
    auto samples = caps.MaxSamples;
    for (const auto& attachment:m_ColorAttachmentSpecifications) {
        const auto supported = attachment.TextureFormat==FramebufferTextureFormat::RED_INTEGER ? caps.MaxIntegerSamples : caps.MaxColorSamples;
        if (attachment.TextureFormat==FramebufferTextureFormat::RED_INTEGER && m_Specification.Samples>1 && m_Specification.Samples>supported)
            throw std::invalid_argument("Integer multisample textures unsupported at requested sample count (limit "+std::to_string(supported)+")");
        samples=std::min(samples,supported);
    }
    if (m_DepthAttachmentSpecification.TextureFormat!=FramebufferTextureFormat::None)
        samples=std::min(samples,caps.MaxDepthSamples);
    if (!m_Specification.Width || !m_Specification.Height || m_Specification.Width>static_cast<uint32_t>(maximum) ||
        m_Specification.Height>static_cast<uint32_t>(maximum) || !m_Specification.Samples ||
        m_Specification.Samples>static_cast<uint32_t>(std::max(samples,1u)) ||
        m_ColorAttachmentSpecifications.size()>static_cast<std::size_t>(std::min(drawBuffers,colorAttachments)))
        throw std::invalid_argument("Framebuffer specification exceeds hardware limits");
    Utils::FramebufferState state;
    const bool restoreDraw=m_RendererID && state.Draw==static_cast<GLint>(m_RendererID);
    const bool restoreRead=m_RendererID && state.Read==static_cast<GLint>(m_RendererID);
    if (restoreDraw) state.Draw=0;
    if (restoreRead) state.Read=0;
    Release();
    const bool multisampled=m_Specification.Samples>1;
    Utils::TextureBindingState textureState(multisampled),resolveTextureState(false);
    GLint unpackBuffer=0; glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    try {
        glGenFramebuffers(1,&m_RendererID);
        if (!m_RendererID) throw std::runtime_error("Failed to create framebuffer");
        glBindFramebuffer(GL_FRAMEBUFFER,m_RendererID);
        m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
        Utils::CreateTextures(multisampled,m_ColorAttachments.data(),static_cast<uint32_t>(m_ColorAttachments.size()));
        for (std::size_t i=0;i<m_ColorAttachments.size();++i) {
            const bool integer=m_ColorAttachmentSpecifications[i].TextureFormat==FramebufferTextureFormat::RED_INTEGER;
            Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples,
                                      integer ? GL_R32I
                                      : m_ColorAttachmentSpecifications[i].TextureFormat ==
                                              FramebufferTextureFormat::RGBA32F
                                          ? GL_RGBA32F
                                      : m_ColorAttachmentSpecifications[i].TextureFormat ==
                                              FramebufferTextureFormat::RGBA16F
                                          ? GL_RGBA16F
                                          : GL_RGBA8,
                                      integer ? GL_RED_INTEGER : GL_RGBA, m_Specification.Width,
                                      m_Specification.Height, static_cast<int>(i));
        }
        if (m_DepthAttachmentSpecification.TextureFormat!=FramebufferTextureFormat::None) {
            Utils::CreateTextures(multisampled,&m_DepthAttachment,1);
            const bool depth32=m_DepthAttachmentSpecification.TextureFormat==FramebufferTextureFormat::DEPTH32F;
            Utils::AttachDepthTexture(m_DepthAttachment,m_Specification.Samples,depth32?GL_DEPTH_COMPONENT32F:GL_DEPTH24_STENCIL8,depth32?GL_DEPTH_ATTACHMENT:GL_DEPTH_STENCIL_ATTACHMENT,
                                     m_Specification.Width,m_Specification.Height);
        }
        Utils::SetDrawBuffers(m_ColorAttachments.size());
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Framebuffer is incomplete");
        if (multisampled && !m_ColorAttachments.empty()) {
            glGenFramebuffers(1,&m_ResolveRendererID);
            if (!m_ResolveRendererID) throw std::runtime_error("Failed to create resolve framebuffer");
            glBindFramebuffer(GL_FRAMEBUFFER,m_ResolveRendererID);
            m_ResolveColorAttachments.resize(m_ColorAttachments.size());
            Utils::CreateTextures(false,m_ResolveColorAttachments.data(),static_cast<uint32_t>(m_ResolveColorAttachments.size()));
            for (std::size_t i=0;i<m_ResolveColorAttachments.size();++i) {
                const bool integer=m_ColorAttachmentSpecifications[i].TextureFormat==FramebufferTextureFormat::RED_INTEGER;
                Utils::AttachColorTexture(m_ResolveColorAttachments[i], 1,
                                          integer ? GL_R32I
                                          : m_ColorAttachmentSpecifications[i].TextureFormat ==
                                                  FramebufferTextureFormat::RGBA32F
                                              ? GL_RGBA32F
                                          : m_ColorAttachmentSpecifications[i].TextureFormat ==
                                                  FramebufferTextureFormat::RGBA16F
                                              ? GL_RGBA16F
                                              : GL_RGBA8,
                                          integer ? GL_RED_INTEGER : GL_RGBA, m_Specification.Width,
                                          m_Specification.Height, static_cast<int>(i));
            }
            Utils::SetDrawBuffers(m_ResolveColorAttachments.size());
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Multisample resolve framebuffer is incomplete");
        }
        if (restoreDraw) { state.Draw=m_RendererID; DisableIntegerBlending(); }
        if (restoreRead) state.Read=m_RendererID;
    } catch (...) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        Release(); throw;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
}

void OpenGLFramebuffer::EncodeOutput(const Ref<Framebuffer>& destination)
{
    if(m_ColorAttachmentSpecifications.empty() || (m_ColorAttachmentSpecifications[0].TextureFormat!=FramebufferTextureFormat::RGBA16F &&
        m_ColorAttachmentSpecifications[0].TextureFormat!=FramebufferTextureFormat::RGBA32F))
        throw std::invalid_argument("Output encoding requires a display-linear RGBA16F or RGBA32F source");
    auto target=std::dynamic_pointer_cast<OpenGLFramebuffer>(destination);
    if(destination && (!target || target.get()==this || target->m_ColorAttachmentSpecifications.size()!=1 ||
        target->m_ColorAttachmentSpecifications[0].TextureFormat!=FramebufferTextureFormat::RGBA8 || target->m_Specification.Samples!=1))
        throw std::invalid_argument("Output encoding destination must be a separate single RGBA8 target");
    Utils::FramebufferState framebuffers;
    GLint program=0,vao=0,active=0,texture=0,sampler=0,viewport[4],polygon[2];GLboolean mask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);glBindSampler(0,0);
    glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_POLYGON_MODE,polygon);glGetBooleani_v(GL_COLOR_WRITEMASK,0,mask);
    const GLenum states[]{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_FRAMEBUFFER_SRGB};
    const GLboolean blend0=glIsEnabledi(GL_BLEND,0);glDisablei(GL_BLEND,0);
    GLboolean enabled[4];for(int i=0;i<4;++i){enabled[i]=glIsEnabled(states[i]);glDisable(states[i]);}
    struct Restore {
        GLint Program,VAO,Active,Texture,Sampler;GLint* Viewport;GLint* Polygon;GLboolean* Mask;const GLenum* States;GLboolean* Enabled;GLboolean Blend0;
        ~Restore(){glUseProgram(Program);glBindVertexArray(VAO);glBindTexture(GL_TEXTURE_2D,Texture);glBindSampler(0,Sampler);glActiveTexture(Active);
            glViewport(Viewport[0],Viewport[1],Viewport[2],Viewport[3]);glPolygonMode(GL_FRONT_AND_BACK,Polygon[0]);
            glColorMaski(0,Mask[0],Mask[1],Mask[2],Mask[3]);if(Blend0)glEnablei(GL_BLEND,0);else glDisablei(GL_BLEND,0);for(int i=0;i<4;++i)if(Enabled[i])glEnable(States[i]);else glDisable(States[i]);}
    } restore{program,vao,active,texture,sampler,viewport,polygon,mask,states,enabled,blend0};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,target?target->m_RendererID:0);
    if(target)glViewport(0,0,target->m_Specification.Width,target->m_Specification.Height);
    glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    if(!m_OutputShader)m_OutputShader=Shader::Create((Resources::Get().Root/"shaders/DisplayOutput.glsl").generic_u8string());
    m_OutputShader->Bind();m_OutputShader->SetInt("u_Linear",0);
    glBindTexture(GL_TEXTURE_2D,GetColorAttachmentRendererID(0));
    if(!m_OutputVAO)glGenVertexArrays(1,&m_OutputVAO);glBindVertexArray(m_OutputVAO);glDrawArrays(GL_TRIANGLES,0,3);
}

void OpenGLFramebuffer::DrawEntityOutlineTo(const Ref<Framebuffer> &destination, int entity,
                                            int radius, glm::vec4 encodedColor)
{
    auto target = std::dynamic_pointer_cast<OpenGLFramebuffer>(destination);
    if (!target || target.get() == this || m_Specification.Samples != 1 ||
        target->m_Specification.Samples != 1 ||
        m_Specification.Width != target->m_Specification.Width ||
        m_Specification.Height != target->m_Specification.Height ||
        m_ColorAttachmentSpecifications.size() < 2 ||
        m_ColorAttachmentSpecifications[1].TextureFormat != FramebufferTextureFormat::RED_INTEGER ||
        target->m_ColorAttachmentSpecifications.size() != 1 ||
        target->m_ColorAttachmentSpecifications[0].TextureFormat !=
            FramebufferTextureFormat::RGBA8 ||
        radius < 1 || radius > 8)
        throw std::invalid_argument("Selection outline needs matching single-sample ID and "
                                    "separate encoded viewport targets; radius 1..8 pixels");
    RenderCommand::StateScope scope;
    auto raster = RenderCommand::GetRasterState();
    raster.DepthTest = raster.DepthWrite = raster.Blend = raster.SRGB = raster.ClipEnabled = false;
    raster.Cull = CullFace::None;
    raster.ColorMask = glm::bvec4(true);
    RenderCommand::SetRasterState(raster);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->m_RendererID);
    glViewport(0, 0, m_Specification.Width, m_Specification.Height);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (!m_OutlineShader)
        m_OutlineShader = Shader::Create(
            (Resources::Get().Root / "shaders/EntityOutline.glsl").generic_u8string());
    m_OutlineShader->Bind();
    m_OutlineShader->SetInt("u_Entities", 0);
    m_OutlineShader->SetInt("u_Selected", entity);
    m_OutlineShader->SetInt("u_Radius", radius);
    m_OutlineShader->SetInt("u_FlipY", 0);
    m_OutlineShader->SetFloat4("u_EncodedColor", encodedColor);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, GetColorAttachmentRendererID(1));
    if (!m_OutputVAO)
        glGenVertexArrays(1, &m_OutputVAO);
    glBindVertexArray(m_OutputVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

glm::vec4 OpenGLFramebuffer::ReadColorPixel(uint32_t attachment, int x, int y)
{
    if (attachment >= m_ColorAttachments.size() ||
        m_ColorAttachmentSpecifications[attachment].TextureFormat ==
            FramebufferTextureFormat::RED_INTEGER ||
        x < 0 || y < 0 || x >= int(m_Specification.Width) || y >= int(m_Specification.Height))
        throw std::runtime_error("Invalid float color readback");
    glm::vec4 color;
    ReadFloatColors(attachment, x, y, 1, 1, &color);
    return color;
}
std::vector<glm::vec4> OpenGLFramebuffer::ReadColorPixels()
{
    if (m_Specification.Samples != 1 || m_ColorAttachmentSpecifications.empty() ||
        m_ColorAttachmentSpecifications[0].TextureFormat != FramebufferTextureFormat::RGBA32F)
        throw std::runtime_error("Bulk HDR readback requires a single-sample RGBA32F attachment");
    std::vector<glm::vec4> pixels(size_t(m_Specification.Width) * m_Specification.Height);
    ReadFloatColors(0, 0, 0, int(m_Specification.Width), int(m_Specification.Height), pixels.data());
    return pixels;
}
void OpenGLFramebuffer::ReadFloatColors(uint32_t attachment, int x, int y, int width, int height, glm::vec4 *pixels)
{
    Resolve();
    Utils::FramebufferState state;
    GLint pack, alignment, oldRead, rowLength, skipRows, skipPixels;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      m_ResolveRendererID ? m_ResolveRendererID : m_RendererID);
    glGetIntegerv(GL_READ_BUFFER, &oldRead);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
    glReadPixels(x, y, width, height, GL_RGBA, GL_FLOAT, pixels);
    glReadBuffer(oldRead);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
}
void OpenGLFramebuffer::ResolveReflectionsTo(const Ref<Framebuffer>& destination,
                                            const ScreenReflectionResolve& parameters)
{
    auto target = std::dynamic_pointer_cast<OpenGLFramebuffer>(destination);
    if (!target || target.get() == this || m_Specification.Samples != 1 ||
        target->m_Specification.Samples != 1 ||
        m_Specification.Width != target->m_Specification.Width ||
        m_Specification.Height != target->m_Specification.Height ||
        m_ColorAttachments.size() != 5 || target->m_ColorAttachments.size() != 2 ||
        !m_DepthAttachment || !target->m_DepthAttachment)
        throw std::runtime_error("SSR requires matching single-sample HDR source and destination");
    RenderCommand::StateScope state;
    auto raster = RenderCommand::GetRasterState();
    raster.DepthTest = raster.DepthWrite = raster.Blend = raster.ClipEnabled = raster.SRGB = false;
    raster.Cull = CullFace::None;
    raster.ColorMask = raster.PickingMask = glm::bvec4(true);
    RenderCommand::SetRasterState(raster);
    Utils::FramebufferState bindings;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->m_RendererID);
    if (!m_ReflectionShader)
        m_ReflectionShader=Shader::Create((Resources::Get().Root/"shaders/ScreenReflections.glsl").generic_u8string());
    if (!m_ReflectionParameters)
        m_ReflectionParameters=UniformBuffer::Create(sizeof(parameters), 5);
    m_ReflectionParameters->Bind();
    m_ReflectionParameters->SetData(&parameters, sizeof(parameters));
    m_ReflectionShader->Bind();
    const char* names[]{"u_Radiance", "u_Identity", "u_Depth", "u_Normals", "u_Weight", "u_Fallback"};
    const uint32_t textures[]{m_ColorAttachments[0], m_ColorAttachments[1], m_DepthAttachment,
        m_ColorAttachments[2], m_ColorAttachments[3], m_ColorAttachments[4]};
    for (unsigned slot=0; slot<6; ++slot) {
        m_ReflectionShader->SetInt(names[slot], int(slot));
        glActiveTexture(GL_TEXTURE0+slot);
        glBindTexture(GL_TEXTURE_2D, textures[slot]);
        glBindSampler(slot, 0);
    }
    if (!m_OutputVAO)
        glGenVertexArrays(1, &m_OutputVAO);
    glBindVertexArray(m_OutputVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    const auto width=m_Specification.Width, height=m_Specification.Height;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                      GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
}
void OpenGLFramebuffer::BindDefocusLayers(Shader& shader, const DepthOfFieldPass* defocus) const
{
    shader.SetInt("u_DefocusDepth", 4);
    shader.SetInt("u_DefocusNear", defocus ? 5 : 0);
    shader.SetInt("u_DefocusFar", defocus ? 6 : 0);
    shader.SetInt("u_DefocusBounds", defocus ? 7 : 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
    glBindSampler(4, 0);
    if (!defocus) return;
    const auto& layers = static_cast<const OpenGLFramebuffer&>(*defocus->Gathered);
    const auto& bounds = static_cast<const OpenGLFramebuffer&>(*defocus->DilatedTiles);
    const GLuint inputs[]{layers.m_ColorAttachments[0], layers.m_ColorAttachments[1], bounds.m_ColorAttachments[0]};
    for (unsigned i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE5 + i);
        glBindTexture(GL_TEXTURE_2D, inputs[i]);
        glBindSampler(5 + i, 0);
    }
}
void OpenGLFramebuffer::BindShutterLayer(Shader& shader, const CameraMotionPass* shutter) const
{
    const bool active = shutter && shutter->Valid;
    shader.SetInt("u_ShutterLayer", active ? 8 : 0);
    if (!active) return;
    const auto& layer = static_cast<const OpenGLFramebuffer&>(*shutter->Filtered);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, layer.m_ColorAttachments[0]);
    glBindSampler(8, 0);
}
void OpenGLFramebuffer::FilterShutter(const CameraMotionPass& shutter)
{
    if (!m_ShutterShader) {
        m_ShutterParameters = UniformBuffer::Create(sizeof(ShutterParameters), 5);
        m_ShutterShader = Shader::Create((Resources::Get().Root / "shaders/ShutterFilter.glsl").generic_u8string());
    }
    const auto& vectors = static_cast<const OpenGLFramebuffer&>(*shutter.Vectors);
    const auto& tiles = static_cast<const OpenGLFramebuffer&>(*shutter.TileMax);
    const auto& neighbors = static_cast<const OpenGLFramebuffer&>(*shutter.NeighborMax);
    const auto& filtered = static_cast<const OpenGLFramebuffer&>(*shutter.Filtered);
    const auto texture = [](unsigned slot, GLuint id) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, id);
        glBindSampler(slot, 0);
    };
    const char* names[]{"u_World", "u_Depth", "u_Entity", "u_PreviousPosition", "u_VectorEntity", "u_Tiles"};
    m_ShutterShader->Bind();
    for (unsigned slot = 0; slot < std::size(names); ++slot)
        m_ShutterShader->SetInt(names[slot], int(slot));
    texture(0, m_ColorAttachments[0]);
    texture(1, m_DepthAttachment);
    texture(2, m_ColorAttachments[1]);
    texture(3, vectors.m_ColorAttachments[0]);
    texture(4, vectors.m_ColorAttachments[1]);
    const OpenGLFramebuffer* outputs[]{&tiles, &neighbors, &filtered};
    const GLuint tileInputs[]{m_ColorAttachments[0], tiles.m_ColorAttachments[0],
        neighbors.m_ColorAttachments[0]};
    m_ShutterParameters->Bind();
    auto params = shutter.Parameters;
    CameraMotionTiming timing;
    if (shutter.Timing)
        timing.WorkingBytes = uint64_t(vectors.m_Specification.Width) * vectors.m_Specification.Height * 24;
    for (unsigned stage = 0; stage < std::size(outputs); ++stage) {
        const auto& output = *outputs[stage];
        GLuint query = 0;
        if (shutter.Timing) {
            glGenQueries(1, &query);
            glBeginQuery(GL_TIME_ELAPSED, query);
        }
        params.Shutter.w = float(stage);
        m_ShutterParameters->SetData(&params, sizeof(params));
        texture(5, tileInputs[stage]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output.m_RendererID);
        glViewport(0, 0, output.m_Specification.Width, output.m_Specification.Height);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (query) {
            glFinish();
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            glDeleteQueries(1, &query);
            timing.Milliseconds[stage] = double(ns) / 1e6;
            timing.WorkingBytes += uint64_t(output.m_Specification.Width) * output.m_Specification.Height * 16;
        }
    }
    if (shutter.Timing) *shutter.Timing = timing;
}
void OpenGLFramebuffer::FilterDefocus(const DepthOfFieldPass& defocus, const CameraMotionPass* shutter)
{
    if (!m_DefocusTiles) {
        const auto shaders = Resources::Get().Root / "shaders";
        auto tiles = Shader::Create((shaders / "DefocusTile.glsl").generic_u8string());
        auto prefilter = Shader::Create((shaders / "DefocusPrefilter.glsl").generic_u8string());
        auto gather = Shader::Create((shaders / "DefocusGather.glsl").generic_u8string());
        m_DefocusParameters = UniformBuffer::Create(sizeof(DefocusParameters), 5);
        m_DefocusTiles = std::move(tiles);
        m_DefocusPrefilter = std::move(prefilter);
        m_DefocusGather = std::move(gather);
    }
    const auto& tiles = static_cast<const OpenGLFramebuffer&>(*defocus.Tiles);
    const auto& dilated = static_cast<const OpenGLFramebuffer&>(*defocus.DilatedTiles);
    const auto& prefiltered = static_cast<const OpenGLFramebuffer&>(*defocus.Prefiltered);
    const auto& layers = static_cast<const OpenGLFramebuffer&>(*defocus.Gathered);
    const auto bind = [](unsigned slot, GLuint texture) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(slot, 0);
    };
    DepthOfFieldTiming timing;
    size_t phase = 0;
    const auto draw = [&](const OpenGLFramebuffer& output) {
        GLuint query = 0;
        if (defocus.Timing) {
            glGenQueries(1, &query);
            glBeginQuery(GL_TIME_ELAPSED, query);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output.m_RendererID);
        glViewport(0, 0, output.m_Specification.Width, output.m_Specification.Height);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (query) {
            // Isolated attribution on Apple's GL driver.
            glFinish();
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            glDeleteQueries(1, &query);
            timing.Milliseconds[phase++] = double(ns) / 1e6;
            timing.WorkingBytes += uint64_t(output.m_Specification.Width) * output.m_Specification.Height *
                output.m_ColorAttachments.size() * 16;
        }
    };
    m_DefocusParameters->Bind();
    auto parameters = defocus.Parameters;
    parameters.Lens.w = 0;
    m_DefocusTiles->Bind();
    m_DefocusTiles->SetInt("u_Input", 0);
    m_DefocusParameters->SetData(&parameters.Lens, sizeof(parameters.Lens));
    bind(0, m_DepthAttachment);
    draw(tiles);
    parameters.Lens.w = 1;
    m_DefocusParameters->SetData(&parameters.Lens, sizeof(parameters.Lens));
    bind(0, tiles.m_ColorAttachments.front());
    draw(dilated);
    m_DefocusPrefilter->Bind();
    parameters.Lens.w = 0;
    const glm::vec4 prefilterParams[]{parameters.Lens,
        glm::vec4(0, 0, shutter && shutter->Valid ? 1.f : 0.f, 0)};
    m_DefocusParameters->SetData(prefilterParams, sizeof(prefilterParams));
    BindShutterLayer(*m_DefocusPrefilter, shutter);
    m_DefocusPrefilter->SetInt("u_Source", 0);
    m_DefocusPrefilter->SetInt("u_Depth", 1);
    m_DefocusPrefilter->SetInt("u_Tiles", 2);
    bind(0, m_ColorAttachments.front());
    bind(1, m_DepthAttachment);
    bind(2, dilated.m_ColorAttachments.front());
    draw(prefiltered);
    m_DefocusGather->Bind();
    m_DefocusParameters->SetData(&parameters, sizeof(parameters));
    m_DefocusGather->SetInt("u_Source", 0);
    m_DefocusGather->SetInt("u_Tiles", 2);
    bind(0, prefiltered.m_ColorAttachments.front());
    draw(layers);
    if (defocus.Timing) *defocus.Timing = timing;
}
uint32_t OpenGLFramebuffer::FilterBloom(const BloomPyramid& bloom, bool timePasses, const DepthOfFieldPass* defocus,
                                        const CameraMotionPass* shutter)
{
    if (!m_BloomShader)
        m_BloomShader = Shader::Create((Resources::Get().Root / "shaders/BloomFilter.glsl").generic_u8string());
    if (!m_BloomParameters)
        m_BloomParameters = UniformBuffer::Create(sizeof(glm::vec4) * 3, 5);
    m_BloomShader->Bind();
    m_BloomShader->SetInt("u_Source", 0);
    m_BloomShader->SetInt("u_Fine", 1);
    BindDefocusLayers(*m_BloomShader, defocus);
    BindShutterLayer(*m_BloomShader, shutter);
    m_BloomParameters->Bind();
    if (!m_OutputVAO)
        glGenVertexArrays(1, &m_OutputVAO);
    glBindVertexArray(m_OutputVAO);
    auto draw = [&](const OpenGLFramebuffer& output, const OpenGLFramebuffer& source,
                    const OpenGLFramebuffer& fine, float mode) {
        const auto& extent = source.GetSpecification();
        const auto& size = output.GetSpecification();
        const glm::vec4 parameters[]{bloom.FilterParameters(mode, extent.Width, extent.Height),
            defocus && mode == 0 ? defocus->Parameters.Lens : glm::vec4(0),
            glm::vec4(0, 0, shutter && shutter->Valid && mode == 0 ? 1.f : 0.f, 0)};
        m_BloomParameters->SetData(parameters, sizeof(parameters));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output.m_RendererID);
        glViewport(0, 0, size.Width, size.Height);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, source.m_ColorAttachments[0]);
        glBindSampler(0, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, fine.m_ColorAttachments[0]);
        glBindSampler(1, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    };
    // Qualification separates Apple GL's batched passes with explicit
    // completion boundaries. Ordinary rendering never enables this path.
    if (timePasses) glBeginQuery(GL_TIME_ELAPSED, m_BloomQueries[0]);
    const auto* source = static_cast<const OpenGLFramebuffer*>(bloom.Levels.front().Down.get());
    draw(*source, *this, *this, 0.f);
    if (timePasses) {
        glFinish();
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_BloomQueries[1]);
    }
    for (size_t i = 1; i < bloom.Levels.size(); ++i) {
        const auto& output = static_cast<const OpenGLFramebuffer&>(*bloom.Levels[i].Down);
        draw(output, *source, *source, 1.f);
        source = &output;
    }
    if (timePasses) {
        glFinish();
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_BloomQueries[2]);
    }
    if (bloom.BuildBloom)
        for (size_t i = bloom.Levels.size() - 1; i > 0; --i) {
            const auto& fine = bloom.Levels[i - 1];
            const auto& output = static_cast<const OpenGLFramebuffer&>(*fine.Up);
            draw(output, *source, static_cast<const OpenGLFramebuffer&>(*fine.Down), 2.f);
            source = &output;
        }
    if (timePasses) {
        glFinish();
        glEndQuery(GL_TIME_ELAPSED);
    }
    return source->m_ColorAttachments[0];
}
uint32_t OpenGLFramebuffer::FilterFlare(const LensEffectsPass& optics, const BloomPyramid& pyramid)
{
    if (!m_FlareShader) {
        m_FlareParameters = UniformBuffer::Create(sizeof(FlareParameters), 5);
        m_FlareShader = Shader::Create((Resources::Get().Root / "shaders/LensFlare.glsl").generic_u8string());
    }
    const auto& input = static_cast<const OpenGLFramebuffer&>(*pyramid.Levels[std::min<size_t>(2, pyramid.Levels.size() - 1)].Down);
    const auto& output = static_cast<const OpenGLFramebuffer&>(*optics.Flare);
    m_FlareShader->Bind();
    m_FlareShader->SetInt("u_Highlights", 0);
    m_FlareParameters->Bind();
    m_FlareParameters->SetData(&optics.Parameters, sizeof(optics.Parameters));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input.m_ColorAttachments[0]);
    glBindSampler(0, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output.m_RendererID);
    glViewport(0, 0, output.m_Specification.Width, output.m_Specification.Height);
    GLuint query = 0;
    if (optics.Timing) {
        glGenQueries(1, &query);
        glBeginQuery(GL_TIME_ELAPSED, query);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (query) {
        glFinish(); // Isolated native benchmark only.
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
        glDeleteQueries(1, &query);
        *optics.Timing = {double(ns) / 1e6, uint64_t(output.m_Specification.Width) * output.m_Specification.Height * 8};
    }
    return output.m_ColorAttachments[0];
}
uint32_t OpenGLFramebuffer::MeterExposure(const ExposureMeterPass& meter)
{
    const auto& histogram = static_cast<const OpenGLFramebuffer&>(*meter.Histogram);
    const auto& current = static_cast<const OpenGLFramebuffer&>(*meter.Current);
    const auto& previous = static_cast<const OpenGLFramebuffer&>(*meter.Previous);
    if (!m_ExposureHistogramShader) {
        const auto shaders = Resources::Get().Root / "shaders";
        auto histogramShader = Shader::Create((shaders / "ExposureHistogram.glsl").generic_u8string());
        auto adaptShader = Shader::Create((shaders / "ExposureAdapt.glsl").generic_u8string());
        auto parameters = UniformBuffer::Create(sizeof(meter.Parameters), 5);
        m_ExposureHistogramShader = std::move(histogramShader);
        m_ExposureAdaptShader = std::move(adaptShader);
        m_ExposureParameters = std::move(parameters);
    }

    const auto bindTexture = [](unsigned unit, GLuint texture) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(unit, 0);
    };
    GLuint query = 0;
    if (meter.Timing) {
        // Isolated trace mode. Apple GL requires completing each phase before
        // ending its elapsed query. The production path never waits here.
        glGenQueries(1, &query);
        glBeginQuery(GL_TIME_ELAPSED, query);
    }
    m_ExposureParameters->Bind();
    m_ExposureParameters->SetData(&meter.Parameters, sizeof(meter.Parameters));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, histogram.m_RendererID);
    glViewport(0, 0, 128, 16);
    const GLfloat zero[4]{};
    glClearBufferfv(GL_COLOR, 0, zero);
    glEnablei(GL_BLEND, 0);
    glBlendEquationSeparatei(0, GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparatei(0, GL_ONE, GL_ONE, GL_ONE, GL_ONE);
    const auto pointSize = glIsEnabled(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    m_ExposureHistogramShader->Bind();
    m_ExposureHistogramShader->SetInt("u_World", 0);
    m_ExposureHistogramShader->SetInt("u_Depth", 1);
    bindTexture(0, m_ColorAttachments[0]);
    bindTexture(1, m_DepthAttachment);
    glDrawArrays(GL_POINTS, 0, GLsizei(std::min(128u, m_Specification.Width) *
                                      std::min(72u, m_Specification.Height)));
    if (!pointSize) glDisable(GL_PROGRAM_POINT_SIZE);
    glDisablei(GL_BLEND, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current.m_RendererID);
    glViewport(0, 0, 1, 1);
    m_ExposureAdaptShader->Bind();
    m_ExposureAdaptShader->SetInt("u_Histogram", 0);
    m_ExposureAdaptShader->SetInt("u_Previous", 1);
    bindTexture(0, histogram.m_ColorAttachments[0]);
    bindTexture(1, previous.m_ColorAttachments[0]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glViewport(0, 0, m_Specification.Width, m_Specification.Height);
    if (query) {
        glFinish();
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
        glDeleteQueries(1, &query);
        *meter.Timing = {double(ns) / 1e6, (128u * 16 + 2) * 16u};
    }
    meter.Submitted->store(true);
    return current.m_ColorAttachments[0];
}
void OpenGLFramebuffer::CompositeWorldTo(const Ref<Framebuffer> &destination, float exposure,
                                         const BloomPyramid* bloom, const ExposureMeterPass* meter,
                                         const DepthOfFieldPass* defocus, const CameraMotionPass* shutter,
                                         const LensEffectsPass* optics)
{
    auto target = std::dynamic_pointer_cast<OpenGLFramebuffer>(destination);
    if (!std::isfinite(exposure) || exposure < -16 || exposure > 16 || !target ||
        target.get() == this || m_Specification.Samples != 1 ||
        target->m_Specification.Samples != 1 ||
        m_Specification.Width != target->m_Specification.Width ||
        m_Specification.Height != target->m_Specification.Height ||
        m_ColorAttachmentSpecifications.size() != 2 ||
        target->m_ColorAttachmentSpecifications.size() != 2 ||
        m_ColorAttachmentSpecifications[0].TextureFormat != FramebufferTextureFormat::RGBA32F ||
        m_ColorAttachmentSpecifications[1].TextureFormat != FramebufferTextureFormat::RED_INTEGER ||
        target->m_ColorAttachmentSpecifications[1].TextureFormat !=
            FramebufferTextureFormat::RED_INTEGER ||
        (target->m_ColorAttachmentSpecifications[0].TextureFormat != FramebufferTextureFormat::RGBA16F &&
         target->m_ColorAttachmentSpecifications[0].TextureFormat != FramebufferTextureFormat::RGBA32F) ||
        !m_DepthAttachment || !target->m_DepthAttachment)
        throw std::runtime_error("World presentation needs matching single-sample HDR/display "
                                 "color, integer ID and depth targets and exposure [-16,16]");
    RenderCommand::StateScope state;
    auto raster = RenderCommand::GetRasterState();
    raster.DepthTest = false;
    raster.DepthWrite = false;
    raster.Blend = false;
    raster.Cull = CullFace::None;
    raster.ClipEnabled = false;
    raster.SRGB = false;
    raster.ColorMask = glm::bvec4(true);
    raster.PickingMask = glm::bvec4(true);
    raster.Viewport = {0, 0, m_Specification.Width, m_Specification.Height};
    RenderCommand::SetRasterState(raster);
    if (!m_OutputVAO)
        glGenVertexArrays(1, &m_OutputVAO);
    glBindVertexArray(m_OutputVAO);
    const auto adaptedGain = meter ? MeterExposure(*meter) : m_ColorAttachments[0];
    const bool motion = shutter && shutter->Valid;
    if (motion) FilterShutter(*shutter);
    if (defocus) FilterDefocus(*defocus, shutter);
    bool timePasses = false;
    if (bloom && bloom->Timing) {
        GLint active = 0;
        glGetQueryiv(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &active);
        if (!active) {
            if (!m_BloomQueries.front()) {
                GLint bits = 0;
                glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &bits);
                if (bits)
                    glGenQueries(GLsizei(m_BloomQueries.size()), m_BloomQueries.data());
            }
            if (m_BloomQueriesPending) {
                GLint ready = 0;
                glGetQueryObjectiv(m_BloomQueries.back(), GL_QUERY_RESULT_AVAILABLE, &ready);
                if (ready) {
                    BloomTiming result;
                    result.WorkingBytes = bloom->TexturePayloadBytes();
                    result.Valid = true;
                    for (size_t i = 0; i < m_BloomQueries.size(); ++i) {
                        GLuint64 ns = 0;
                        glGetQueryObjectui64v(m_BloomQueries[i], GL_QUERY_RESULT, &ns);
                        result.Milliseconds[i] = double(ns) / 1e6;
                    }
                    *bloom->Timing = result;
                    m_BloomQueriesPending = false;
                }
            }
            timePasses = m_BloomQueries.front() && !m_BloomQueriesPending;
        }
    }
    const auto glow = bloom ? FilterBloom(*bloom, timePasses, defocus, shutter) : m_ColorAttachments[0];
    const auto flare = optics && optics->Flare ? FilterFlare(*optics, *bloom) : m_ColorAttachments[0];
    if (bloom || defocus || motion)
        glViewport(0, 0, m_Specification.Width, m_Specification.Height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->m_RendererID);
    const GLenum outputs[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, outputs);
    if (!m_ToneShader)
        m_ToneShader = Shader::Create(
            (Resources::Get().Root / "shaders/WorldPresentation.glsl").generic_u8string());
    if (!m_ToneParameters)
        m_ToneParameters = UniformBuffer::Create(sizeof(glm::vec4) * 5, 5);
    const auto lens = optics ? optics->Presentation : LensPresentation{};
    const glm::vec4 parameters[]{glm::vec4(exposure, bloom ? bloom->Intensity : 0.f, meter ? 1.f : 0.f, motion ? 1.f : 0.f),
        defocus ? defocus->Parameters.Lens : glm::vec4(0), lens.Sensor, lens.Gate, lens.Dirt};
    m_ToneParameters->Bind();
    m_ToneParameters->SetData(parameters, sizeof(parameters));
    m_ToneShader->Bind();
    m_ToneShader->SetInt("u_World", 0);
    m_ToneShader->SetInt("u_Picking", 1);
    m_ToneShader->SetInt("u_Bloom", 2);
    m_ToneShader->SetInt("u_AdaptedGain", 3);
    m_ToneShader->SetInt("u_Flare", optics && optics->Flare ? 9 : 0);
    m_ToneShader->SetInt("u_Dirt", optics && optics->DirtTexture ? 10 : 0);
    if (optics && optics->Flare) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, flare);
        glBindSampler(9, 0);
    }
    if (optics && optics->DirtTexture) optics->DirtTexture->Bind(10);
    BindDefocusLayers(*m_ToneShader, defocus);
    BindShutterLayer(*m_ToneShader, shutter);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, adaptedGain);
    glBindSampler(3, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glow);
    glBindSampler(2, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[0]);
    glBindSampler(0, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[1]);
    glBindSampler(1, 0);
    glBindVertexArray(m_OutputVAO);
    const bool warp = lens.Sensor.z != 0.f;
    if (warp) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_ALWAYS);
    }
    if (timePasses) glBeginQuery(GL_TIME_ELAPSED, m_BloomQueries[3]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (timePasses) {
        glFinish();
        glEndQuery(GL_TIME_ELAPSED);
        m_BloomQueriesPending = true;
    }
    // IDs copy via integer texelFetch in the same pass, without filtering or conversion.
    // Native M4 rejects an otherwise matching R32I color blit. Copy the complete
    // D24S8 attachment: a masked depth-only blit can round through float on Mesa,
    // whereas the full matching packed format is eligible for a bit-preserving copy.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
    glBlitFramebuffer(0, 0, m_Specification.Width, m_Specification.Height, 0, 0,
                      m_Specification.Width, m_Specification.Height,
                      (warp ? 0 : GL_DEPTH_BUFFER_BIT) | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    if (shutter) shutter->Submitted->store(true);
}

void OpenGLFramebuffer::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER,m_RendererID);
    DisableIntegerBlending();
    glViewport(0,0,m_Specification.Width,m_Specification.Height);
}
void OpenGLFramebuffer::DisableIntegerBlending()
{
    // Integer outputs have replacement semantics. Explicit indexed state avoids
    // native Intel/Mesa producing zero IDs when blending is globally enabled.
    if (m_PreviousBlending.empty()) {
        for (uint32_t i=0;i<m_ColorAttachmentSpecifications.size();++i)
            if (m_ColorAttachmentSpecifications[i].TextureFormat==FramebufferTextureFormat::RED_INTEGER || i>=2)
                m_PreviousBlending.emplace_back(i,glIsEnabledi(GL_BLEND,i)==GL_TRUE);
    }
    for (const auto& [index,enabled]:m_PreviousBlending) glDisablei(GL_BLEND,index);
}
void OpenGLFramebuffer::RestoreBlending()
{
    for (const auto& [index,enabled]:m_PreviousBlending)
        if (enabled) glEnablei(GL_BLEND,index); else glDisablei(GL_BLEND,index);
    m_PreviousBlending.clear();
}
void OpenGLFramebuffer::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER,0); RestoreBlending(); }
void OpenGLFramebuffer::Resize(uint32_t width,uint32_t height)
{
    if (!width || !height || width>s_MaxFramebufferSize || height>s_MaxFramebufferSize) {
        HZ_CORE_WARN("Attempted to resize framebuffer to {}, {}",width,height); return;
    }
    m_Specification.Width=width; m_Specification.Height=height;
    Invalidate();
}

void OpenGLFramebuffer::Resolve() const
{
    if (!m_ResolveRendererID) return;
    Utils::FramebufferState state;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,m_RendererID);
    GLint previousRead=0; glGetIntegerv(GL_READ_BUFFER,&previousRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_ResolveRendererID);
    const GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST); glDisable(GL_SCISSOR_TEST);
    for (std::size_t i=0;i<m_ColorAttachments.size();++i) {
        glReadBuffer(GL_COLOR_ATTACHMENT0+static_cast<GLenum>(i));
        glDrawBuffer(GL_COLOR_ATTACHMENT0+static_cast<GLenum>(i));
        glBlitFramebuffer(0,0,m_Specification.Width,m_Specification.Height,0,0,m_Specification.Width,m_Specification.Height,
                          GL_COLOR_BUFFER_BIT,GL_NEAREST);
    }
    Utils::SetDrawBuffers(m_ColorAttachments.size());
    glReadBuffer(previousRead);
    if (scissor) glEnable(GL_SCISSOR_TEST);
}

int OpenGLFramebuffer::ReadPixel(uint32_t index,int x,int y)
{
    if (index>=m_ColorAttachments.size()) throw std::out_of_range("Framebuffer attachment index");
    if (m_ColorAttachmentSpecifications[index].TextureFormat!=FramebufferTextureFormat::RED_INTEGER)
        throw std::invalid_argument("ReadPixel requires an integer picking attachment");
    if (x<0 || y<0 || x>=static_cast<int>(m_Specification.Width) || y>=static_cast<int>(m_Specification.Height))
        throw std::out_of_range("Framebuffer pixel coordinates");
    Resolve();
    Utils::FramebufferState state;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,m_ResolveRendererID ? m_ResolveRendererID : m_RendererID);
    GLint previousRead=0,packBuffer=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
    glGetIntegerv(GL_READ_BUFFER,&previousRead);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT,&alignment); glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows); glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    glPixelStorei(GL_PACK_ALIGNMENT,4); glPixelStorei(GL_PACK_ROW_LENGTH,0);
    glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    glReadBuffer(GL_COLOR_ATTACHMENT0+index);
    int pixel=0; glReadPixels(x,y,1,1,GL_RED_INTEGER,GL_INT,&pixel);
    glReadBuffer(previousRead); glBindBuffer(GL_PIXEL_PACK_BUFFER,packBuffer);
    glPixelStorei(GL_PACK_ALIGNMENT,alignment); glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS,skipRows); glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
    return pixel;
}

void OpenGLFramebuffer::ClearAttachment(uint32_t index,int value)
{
    if (index>=m_ColorAttachments.size()) throw std::out_of_range("Framebuffer attachment index");
    Utils::FramebufferState state;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_RendererID);
    const auto maximum=OpenGLCapabilities::Get().MaxDrawBuffers;
    std::vector<GLenum> previous(static_cast<std::size_t>(maximum));
    for (uint32_t i=0;i<maximum;++i) { GLint buffer=0; glGetIntegerv(GL_DRAW_BUFFER0+i,&buffer); previous[i]=buffer; }
    GLboolean mask[4]; glGetBooleani_v(GL_COLOR_WRITEMASK,0,mask);
    const GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST); glDisable(GL_SCISSOR_TEST);
    glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDrawBuffer(GL_COLOR_ATTACHMENT0+index);
    if (m_ColorAttachmentSpecifications[index].TextureFormat==FramebufferTextureFormat::RED_INTEGER) {
        const GLint values[4]={value,value,value,value}; glClearBufferiv(GL_COLOR,0,values);
    } else {
        const GLfloat v=static_cast<GLfloat>(value); const GLfloat values[4]={v,v,v,v}; glClearBufferfv(GL_COLOR,0,values);
    }
    glDrawBuffers(maximum,previous.data()); glColorMaski(0,mask[0],mask[1],mask[2],mask[3]);
    if (scissor) glEnable(GL_SCISSOR_TEST);
}
uint32_t OpenGLFramebuffer::GetColorAttachmentRendererID(uint32_t index) const
{
    if (index>=m_ColorAttachments.size()) throw std::out_of_range("Framebuffer attachment index");
    Resolve();
    return m_ResolveRendererID ? m_ResolveColorAttachments[index] : m_ColorAttachments[index];
}
}

namespace Hazel {
void OpenGLFramebuffer::PrepareShadowDepth(const Ref<Framebuffer>& depth, ShadowMapKind kind)
{
    auto source=std::dynamic_pointer_cast<OpenGLFramebuffer>(depth);
    GLint bound=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&bound);
    if(!source || source.get()==this || bound!=GLint(m_RendererID) || !source->m_ColorAttachments.empty() ||
       !source->m_DepthAttachment || source->m_Specification.Samples!=1)
        throw std::logic_error("Shadows require a distinct depth-only producer and bound lighting view");
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    const GLuint binding = kind == ShadowMapKind::Directional ? 10 : kind == ShadowMapKind::Spot ? 11 : 12;
    glActiveTexture(GL_TEXTURE0 + binding);
    glBindTexture(GL_TEXTURE_2D, source->m_DepthAttachment);
    glBindSampler(binding, 0);
    glActiveTexture(active);
}
}
