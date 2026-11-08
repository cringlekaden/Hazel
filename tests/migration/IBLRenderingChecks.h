#pragma once
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "IBLReferenceMath.h"
#include "IBLMipChecks.h"
#include "IBLQualityLobeChecks.h"
#include "IBLSurfaceRenderingChecks.h"
#include <glm/gtc/constants.hpp>
static void IBLRenderingChecks()
{
    SurfaceReferenceChecks();
    auto diagnosticMips = IBLMipChecks();
    IBLQualityLobeChecks();
    RenderCommand::StateScope testState;
    GLint originalSwap = 0;
    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &originalSwap);
    struct RestoreSwap
    {
        GLint Value;
        ~RestoreSwap()
        {
            glPixelStorei(GL_UNPACK_SWAP_BYTES, Value);
        }
    } restoreSwap{originalSwap};
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
    const auto selector = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    if (selector.empty())
        throw std::runtime_error(
            "IBL GPU qualification requires the maintained Rendering Reference selector");
    auto project = Project::LoadCandidate(selector);
    auto root = project->GetAssetRoot();
    auto constant = CreateRef<PreparedEnvironment>(
        ReadPreparedEnvironment(root, "Reference/IBL_v1/Constant.henv"));
    auto axes = CreateRef<PreparedEnvironment>(
        ReadPreparedEnvironment(root, "Reference/IBL_v1/AsymmetricAxes.henv"));
    Fixture fixture;
    NativeStarter::Generate(fixture.Directory);
    auto assets = CreateRef<ProjectAssets>(fixture.Directory);
    auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    Check(bool(mesh.Accepted), "IBL native geometry missing");
    FramebufferSpecification spec;
    spec.Width = spec.Height = 128;
    spec.Attachments = {FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto display = Framebuffer::Create(spec);
    SceneRenderView view;
    SceneSubmission frame;
    frame.Assets = assets;
    frame.Presentation.SceneReferred = true;
    frame.Environment = constant;
    frame.Presentation.Environment.Asset = "Reference/IBL_v1/Constant.henv";
    auto camera = [&](glm::vec3 d) {
        auto up = std::abs(d.y) > .9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        frame.Camera.Orthographic = true;
        frame.Camera.ToCamera = d;
        frame.Camera.Position = d * 3.f;
        frame.Camera.ViewProjection =
            glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 10.f) * glm::lookAt(d * 3.f, glm::vec3(0), up);
    };
    camera({0, 0, 1});
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.18f, .4f, .7f, 1};
    SubmittedMesh draw;
    draw.Geometry = mesh.Accepted;
    draw.Materials = {material};
    draw.Picking = 123;
    frame.Meshes = {draw};
    auto render = [&](
                      const std::function<void()> &world = [] {},
                      const std::function<void()> &ui = [] {}) {
        for (auto &m : frame.Meshes)
            m.SetWorld(m.World);
        display->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        display->ClearAttachment(1, -1);
        SceneRenderer::Render(frame, view, display, world, ui);
        Check(glGetError() == GL_NO_ERROR, "IBL OpenGL error");
    };
    // Constant furnace: independently integrate the shader's single-scattering
    // recipe. .003 covers the admitted .01 LUT bound times C=.25 plus FP32.
    for (float r : {0.f, .25f, .5f, 1.f})
        for (double nv : {.05, .25, .5, 1.})
        {
            frame.Camera.ToCamera = glm::vec3(std::sqrt(1 - nv * nv), 0, nv);
            auto ab = IBLReferenceIntegral(nv, r);
            for (float metallic : {0.f, .5f, 1.f})
            {
                material->Roughness = r;
                material->Metallic = metallic;
                frame.Light.reset();
                render();
                auto base = glm::dvec3(material->BaseColor),
                     f0 = glm::mix(glm::dvec3(.04), base, double(metallic));
                auto fview = f0 + (1. - f0) * std::pow(1 - nv, 5);
                auto indirect =
                    .25 * ((1 - double(metallic)) * (1. - fview) * base + f0 * ab.x + ab.y);
                NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), indirect, .003,
                        "Independent FP64 IBL reconstruction / furnace");
                Check(display->ReadPixel(1, 64, 64) == 123, "IBL mesh picking");
                auto L = glm::normalize(glm::vec3(.3f, .4f, 1));
                frame.Light = SubmittedDirectional{L, glm::vec3(2)};
                render();
                auto direct = OracleBRDF(base, metallic, r, {0, 0, 1},
                                         glm::dvec3(frame.Camera.ToCamera), glm::dvec3(L)) *
                              2.;
                NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), indirect + direct, .003,
                        "Direct plus indirect radiance");
            }
        }
    GLint restoredSwap = 0;
    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &restoredSwap);
    Check(restoredSwap == GL_TRUE, "Float cube/LUT upload leaked unpack-byte state");
    glPixelStorei(GL_UNPACK_SWAP_BYTES, originalSwap);
    material->Alpha = MaterialAlpha::Mask;
    material->BaseColor.a = .1f;
    render();
    Check(display->ReadPixel(1, 64, 64) == -1, "Masked IBL surface wrote picking over the sky");
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), {.25, .25, .25}, 1e-6,
            "Masked IBL hole did not reveal the environment");
    auto behind = draw;
    auto unlit = CreateRef<MaterialDefinition>();
    unlit->Model = MaterialModel::Unlit;
    unlit->BaseColor = {.1f, .2f, .3f, 1};
    behind.Materials = {unlit};
    behind.Picking = 321;
    behind.World = glm::translate(glm::mat4(1), {0, 0, -.5f});
    frame.Meshes = {draw, behind};
    render();
    Check(display->ReadPixel(1, 64, 64) == 321, "Masked IBL surface occluded rear picking/depth");
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), {.1, .2, .3}, 1e-6,
            "Environment lit an unlit rear surface");
    frame.Meshes = {draw};
    material->Alpha = MaterialAlpha::Opaque;
    material->BaseColor.a = 1;
    frame.Camera.ToCamera = {0, 0, 1};
    material->Metallic = 0;
    material->Roughness = .5f;
    std::string tga(18, '\0');
    tga[2] = 2;
    tga[12] = tga[14] = 1;
    tga[16] = 32;
    tga[17] = 0x28;
    tga += char(255);
    tga += char(255);
    tga += char(64);
    tga += char(255);
    FileSystem::WriteNewFile(fixture.Directory / "AO.tga", tga);
    material->Textures[3].Image = "AO.tga";
    material->OcclusionStrength = 0;
    render();
    auto unoccluded = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    frame.Presentation.Environment.Lighting = false;
    render();
    auto direct = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    frame.Presentation.Environment.Lighting = true;
    material->OcclusionStrength = 1;
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), direct + (unoccluded - direct) * (64. / 255),
            .00001, "AO must affect indirect terms only");
    material->Textures[3].Image.clear();
    frame.Light.reset();
    render();
    auto world = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    for (float ev : {-2.f, 2.f})
    {
        frame.Presentation.Exposure = ev;
        render();
        NearRGB(display->ReadColorPixel(0, 64, 64), OracleNeutral(world * std::exp2(double(ev))),
                .001, "IBL exposure and one Neutral presentation");
    }
    auto ui = [&] {
        auto state = RenderCommand::GetRasterState();
        state.DepthTest = false;
        state.Blend = true;
        RenderCommand::SetRasterState(state);
        Renderer2D::BeginScene(glm::mat4(1));
        Renderer2D::DrawQuad(Transform(), {.25f, .5f, .75f, 1}, 456);
        Renderer2D::EndScene();
    };
    for (float ev : {-8.f, 8.f})
    {
        frame.Presentation.Exposure = ev;
        render([] {}, ui);
        NearRGB(display->ReadColorPixel(0, 64, 64), {.25, .5, .75}, .001,
                "IBL exposure changed independent UI");
        Check(display->ReadPixel(1, 64, 64) == 456, "IBL UI picking");
    }
    frame.Presentation.Exposure = 0;
    // Axis-linear HDR: exact sky direction and E/pi = 1 + 2*N/3. RGBE source
    // quantization contributes <=.008 radiance; cube/LUT interpolation is separate.
    frame.Environment = axes;
    frame.Presentation.Environment.Asset = "Reference/IBL_v1/AsymmetricAxes.henv";
    material->BaseColor = {.18f, .18f, .18f, 1};
    for (float yaw : {0.f, glm::half_pi<float>()})
        for (auto d :
             {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
              glm::vec3(0, 0, 1), glm::vec3(0, 0, -1), glm::normalize(glm::vec3(1, 1, 1)),
              glm::normalize(glm::vec3(1, 0, 1)), glm::normalize(glm::vec3(1.0001f, 0, 1)),
              glm::normalize(glm::vec3(1, 0, 1.0001f))})
        {
            frame.Presentation.Environment.Yaw = yaw;
            camera(d);
            frame.Meshes.clear();
            render();
            auto lookup = [&](glm::dvec3 v) {
                double c = std::cos(yaw), s = std::sin(yaw);
                return glm::dvec3(c * v.x - s * v.z, v.y, s * v.x + c * v.z);
            };
            NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), 1. + lookup(-glm::dvec3(d)), .009,
                    "Asymmetric sky faces / yaw");
            Check(display->ReadPixel(1, 64, 64) == -1, "Environment sky wrote picking");
            auto up = std::abs(d.y) > .9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            draw.World = glm::inverse(glm::lookAt(glm::vec3(0), -d, up));
            frame.Meshes = {draw};
            frame.Debug = SurfaceView::DiffuseIBL;
            material->Metallic = 0;
            render();
            auto expected = (1. + lookup(glm::dvec3(d)) * (2. / 3)) * .18 * .96;
            NearRGB(display->ReadColorPixel(0, 64, 64), expected, .003,
                    "Diffuse and sky orientation disagreement");
            frame.Debug = SurfaceView::SpecularIBL;
            material->Metallic = 1;
            material->Roughness = 0;
            render();
            NearRGB(display->ReadColorPixel(0, 64, 64), (1. + lookup(glm::dvec3(d))) * .18, .003,
                    "Mirror / sky cube orientation or yaw disagreement");
            frame.Debug = SurfaceView::Shaded;
        }
    camera({0, 0, 1});
    frame.Presentation.Environment.Yaw = 0;
    frame.Presentation.Environment.Gain = 2;
    render();
    auto gained = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    frame.Presentation.Environment.Gain = 1;
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), gained / 2., .00001,
            "Environment gain must scale radiance without rebake");
    // Indirect lighting consumes the same mapped, mirrored/nonuniform TBN as
    // the qualified direct path. A linear +Y data normal must tilt irradiance.
    std::string normalPixels(18,'\0');normalPixels[2]=2;normalPixels[12]=normalPixels[14]=1;normalPixels[16]=32;normalPixels[17]=0x28;normalPixels+=char(255);normalPixels+=char(180);normalPixels+=char(220);normalPixels+=char(255);FileSystem::WriteNewFile(fixture.Directory/"Normal.tga",normalPixels);
    camera({0,0,1});frame.Presentation.Environment.Yaw=0;frame.Presentation.Environment.Gain=1;frame.Debug=SurfaceView::DiffuseIBL;material->BaseColor=glm::vec4(.18f,.18f,.18f,1);material->Metallic=0;material->Textures[2].Image="Normal.tga";
    for(float mirror:{1.f,-1.f}){draw.World=glm::scale(glm::mat4(1),{mirror,.5f,2});frame.Meshes={draw};render();auto N=glm::normalize(glm::dvec3(mirror*(2.*220/255-1),-(2.*180/255-1),1));double f=.04+.96*std::pow(1-N.z,5);NearRGB(display->ReadColorPixel(0,64,64),(1.+N*(2./3))*.18*(1-f),.003,"Indirect mapped normal / signed nonuniform TBN");}
    material->Textures[2].Image.clear();frame.Debug=SurfaceView::Shaded;
    // A photographic high-frequency source supplies an independent irradiance
    // and rough-conductor integral, separate from split-sum approximation error.
    auto photo = CreateRef<PreparedEnvironment>(
        ReadPreparedEnvironment(root, "Reference/IBL_v1/Photo.henv"));
    auto image = DecodeRadianceHDR(
        ReadAssetBytes(root / "Reference/IBL_v1/Sources/Photo.hdr", 64 * 1024 * 1024));
    auto imageSample = [&](glm::dvec3 d) {
        const double pi = std::acos(-1.);
        double xx = (.5 + std::atan2(d.z, d.x) / (2 * pi)) * image.Width - .5,
               yy = std::acos(std::clamp(d.y, -1., 1.)) / pi * image.Height - .5;
        int ix = int(std::floor(xx)), iy = int(std::floor(yy));
        auto fetch = [&](int x, int y) {
            x = (x % int(image.Width) + image.Width) % image.Width;
            y = std::clamp(y, 0, int(image.Height) - 1);
            size_t p = (size_t(y) * image.Width + x) * 3;
            return glm::dvec3(image.RGB[p], image.RGB[p + 1], image.RGB[p + 2]);
        };
        return glm::mix(glm::mix(fetch(ix, iy), fetch(ix + 1, iy), xx - ix),
                        glm::mix(fetch(ix, iy + 1), fetch(ix + 1, iy + 1), xx - ix), yy - iy);
    };
    auto photographicIntegral = [&](unsigned n) {
        std::pair<glm::dvec3, glm::dvec3> result{glm::dvec3(0), glm::dvec3(0)};
        const double pi = std::acos(-1.);
        for (unsigned y = 0; y < n; ++y)
        {
            double nl = (y + .5) / n, radius = std::sqrt(1 - nl * nl);
            for (unsigned x = 0; x < 2 * n; ++x)
            {
                double phi = 2 * pi * (x + .5) / (2 * n);
                auto incoming = imageSample({radius * std::cos(phi), radius * std::sin(phi), nl});
                result.first += incoming * (2 * nl / (2 * n * n));
                result.second += incoming * (nl / ((1 + nl) * 2 * n * n));
            }
        }
        return result;
    };
    auto coarse = photographicIntegral(512), fine = photographicIntegral(1024);
    double convergence =
        glm::length(fine.first - coarse.first) / std::max(glm::length(fine.first), 1e-9);
    Check(convergence < .002, "Photographic independent quadrature did not converge");
    const auto &roughMip = photo->Specular.Mips.back();
    glm::dvec3 baked(roughMip[12], roughMip[13], roughMip[14]); // +Z, 1x1 face
    double prefilterError =
        glm::length(baked - fine.first) / std::max(glm::length(fine.first), 1e-9);
    Check(prefilterError < .02, "Photographic roughest GGX prefilter exceeds 2% integral budget");
    frame.Environment = photo;
    frame.Presentation.Environment.Asset = "Reference/IBL_v1/Photo.henv";
    frame.Presentation.Environment.Gain = 1;
    frame.Presentation.Environment.Yaw = 0;
    camera({0, 0, 1});
    draw.World = glm::mat4(1);
    frame.Meshes = {draw};
    material->BaseColor = glm::vec4(1);
    material->Metallic = 1;
    material->Roughness = 1;
    render();
    auto physical = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    double approximation =
        glm::length(physical - fine.second) / std::max(glm::length(fine.second), 1e-9);
    std::cout << "IBL Photo FP64 quadrature convergence " << convergence
              << "; roughest prefilter relative error " << prefilterError
              << "; split-sum versus full rough-conductor relative difference " << approximation
              << " before exposure/tone (model approximation, not an implementation tolerance)\n";
    frame.Environment = axes;
    frame.Presentation.Environment.Asset = "Reference/IBL_v1/AsymmetricAxes.henv";
    // A complete typed default binding is still required after an environment
    // frame. Failure/retry must restore both cube bindings and seamless state.
    render();
    auto samplerBaseline = glm::dvec3(view.Radiance->ReadColorPixel(0, 64, 64));
    GLuint hostileSampler = 0;
    glGenSamplers(1, &hostileSampler);
    glSamplerParameterf(hostileSampler, GL_TEXTURE_MAX_LOD, 0);
    for (unsigned i = 5; i < 8; ++i)
        glBindSampler(i, hostileSampler);
    auto realized = assets->RealizeEnvironment(axes);
    realized->Background->Bind(11);
    GLint priorCube = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &priorCube);
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    for (bool fail : {true, false})
    {
        bool rejected = false;
        try
        {
            render([&] {
                if (fail)
                    throw std::runtime_error("Injected IBL producer failure");
            });
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        Check(rejected == fail, "IBL error/retry contract");
        if (!fail)
            NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), samplerBaseline, 1e-6,
                    "Foreign sampler overrode typed IBL sampling");
        for (unsigned slot = 5; slot < 8; ++slot)
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            GLint sampler;
            glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
            Check(GLuint(sampler) == hostileSampler, "IBL sampler bindings not restored");
        }
        glActiveTexture(GL_TEXTURE11);
        GLint active = 0, cube = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &cube);
        Check(active == GL_TEXTURE11 && cube == priorCube &&
                  !glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS),
              "IBL pass leaked cube/seamless state");
    }
    for (unsigned slot = 5; slot < 8; ++slot)
        glBindSampler(slot, 0);
    glDeleteSamplers(1, &hostileSampler);
    frame.Environment.reset();
    frame.Presentation.Environment = {};
    frame.Light.reset();
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), {}, 1e-7,
            "Absent environment synthesized ambient or retained stale bindings");
    // Now prove the actual PBR shader's roughness-to-LOD mapping and floor.
    auto diagnostic = CreateRef<PreparedEnvironment>(*constant);
    diagnostic->Identity = "explicit-level-diagnostic";
    diagnostic->Generation = UUID();
    diagnostic->Specular = diagnosticMips;
    // The terminal GGX kernel is E/pi. Supply matching dense cosine data;
    // keep every deliberately distinct level and the original .0001 LOD gate.
    diagnostic->Irradiance.Side=32;
    diagnostic->Irradiance.Mips={std::vector<float>(32*32*18)};
    for(unsigned face=0;face<6;++face)for(unsigned i=0;i<32*32;++i)for(unsigned c=0;c<3;++c)
        diagnostic->Irradiance.Mips[0][(face*32*32+i)*3+c]=float(glm::pi<double>()*(1+.03125*face+.00390625*c));
    for (size_t i=0;i<diagnostic->BRDF.Mips[0].size();i+=2)
    { diagnostic->BRDF.Mips[0][i]=1;diagnostic->BRDF.Mips[0][i+1]=0; }
    frame.Environment=diagnostic;
    frame.Presentation.Environment.Lighting=true;
    frame.Presentation.Environment.Background=false;
    frame.Presentation.Environment.Gain=1;
    frame.Presentation.Environment.Yaw=0;
    frame.Debug=SurfaceView::Shaded;
    material->BaseColor={1,1,1,1};material->Metallic=1;material->Textures={};
    draw.World=glm::mat4(1);frame.Meshes={draw};
    camera({0,0,1});
    for (float r : {0.f,.045f,.25f,.5f,.9f,1.f})
    {
        material->Roughness=r;
        render();
        double level=std::max(.045f,r)*3;
        glm::dvec3 expected(.25*(level+1)+.03125*4);
        expected.y+=.00390625;expected.z+=.0078125;
        NearRGB(view.Radiance->ReadColorPixel(0,64,64),expected,.0001,
            "Actual PBR roughness/floor/fractional explicit mip selection");
    }
    IBLSurfaceRenderingChecks(frame,draw,display);
    display->Unbind();
    std::cout << "PASS: IBL_v1 actual GL readbacks, independent FP64 furnace/reconstruction, "
                 "direct+IBL, AO, sky/diffuse/mirror faces/yaw, gain/exposure/UI/picking and "
                 "cube-state failure restoration\n";
}
