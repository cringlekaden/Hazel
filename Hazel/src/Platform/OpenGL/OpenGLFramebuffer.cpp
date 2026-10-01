// Adapted upstream framebuffer: binding-based 4.1 allocation, clearing and MSAA resolve.
#include "hzpch.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include <glad/glad.h>
#include <algorithm>
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
static bool IsDepthFormat(FramebufferTextureFormat format) { return format==FramebufferTextureFormat::DEPTH24STENCIL8; }
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
        glTexImage2D(GL_TEXTURE_2D,0,internalFormat,width,height,0,format,format==GL_RED_INTEGER ? GL_INT : GL_UNSIGNED_BYTE,nullptr);
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
        glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,GL_DEPTH_STENCIL,GL_UNSIGNED_INT_24_8,nullptr);
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
            if (attachment.TextureFormat!=FramebufferTextureFormat::RGBA8 && attachment.TextureFormat!=FramebufferTextureFormat::RED_INTEGER)
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
OpenGLFramebuffer::~OpenGLFramebuffer() { Release(); }

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
            Utils::AttachColorTexture(m_ColorAttachments[i],m_Specification.Samples,integer ? GL_R32I : GL_RGBA8,
                                     integer ? GL_RED_INTEGER : GL_RGBA,m_Specification.Width,m_Specification.Height,static_cast<int>(i));
        }
        if (m_DepthAttachmentSpecification.TextureFormat!=FramebufferTextureFormat::None) {
            Utils::CreateTextures(multisampled,&m_DepthAttachment,1);
            Utils::AttachDepthTexture(m_DepthAttachment,m_Specification.Samples,GL_DEPTH24_STENCIL8,GL_DEPTH_STENCIL_ATTACHMENT,
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
                Utils::AttachColorTexture(m_ResolveColorAttachments[i],1,integer ? GL_R32I : GL_RGBA8,
                                         integer ? GL_RED_INTEGER : GL_RGBA,m_Specification.Width,m_Specification.Height,static_cast<int>(i));
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
            if (m_ColorAttachmentSpecifications[i].TextureFormat==FramebufferTextureFormat::RED_INTEGER)
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
