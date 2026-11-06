// Direct_v1 GPU contract. FP64 oracle below is independent of shader helpers.
#include "Hazel/Assets/NativeStarter.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Renderer/SceneRenderer.h"
#include <cmath>
static glm::dvec3 OracleBRDF(glm::dvec3 base, double metal, double rough, glm::dvec3 n,
                             glm::dvec3 v, glm::dvec3 l)
{
    const double pi = std::acos(-1.0);
    double nl = glm::dot(n, l), nv = glm::dot(n, v);
    if (nl <= 0 || nv <= 0)
        return {};
    auto h = glm::normalize(v + l);
    double nh = glm::dot(n, h), vh = glm::dot(v, h), alpha = std::max(.045, rough);
    alpha *= alpha;
    double alpha2 = alpha * alpha;
    double d = alpha2 / (pi * std::pow(1 + (alpha2 - 1) * nh * nh, 2));
    double lambdaV = std::sqrt(1 + alpha2 * (1 / (nv * nv) - 1)),
           lambdaL = std::sqrt(1 + alpha2 * (1 / (nl * nl) - 1));
    double visibility = 1 / (2 * nl * nv * (lambdaV + lambdaL));
    auto f0 = glm::mix(glm::dvec3(.04), base, metal);
    auto f = f0 + (1.0 - f0) * std::pow(1 - vh, 5);
    return ((1 - metal) * (1.0 - f) * base / pi + d * visibility * f) * nl;
}
static glm::dvec3 OracleNeutral(glm::dvec3 value)
{
    double minimum = std::min({value.r, value.g, value.b});
    double offset = minimum < .08 ? minimum - 6.25 * minimum * minimum : .04;
    value -= offset;
    double peak = std::max({value.r, value.g, value.b});
    if (peak < .76)
        return value;
    double compressed = 1 - .24 * .24 / (peak + .24 - .76);
    auto scaled = value * (compressed / peak);
    double amount = 1 - 1 / (.15 * (peak - compressed) + 1);
    return glm::mix(scaled, glm::dvec3(compressed), amount);
}
static void NearRGB(glm::vec3 actual, glm::dvec3 expected, double tolerance, const char *message)
{
    for (int c = 0; c < 3; ++c)
        if (!std::isfinite(actual[c]) || std::abs(double(actual[c]) - expected[c]) > tolerance)
        {
            std::cerr << message << " channel " << c << " actual=" << actual[c]
                      << " expected=" << expected[c] << '\n';
            throw std::runtime_error(message);
        }
}
static void DirectEnergyChecks()
{
    static bool checked = false;
    if (checked)
        return;
    checked = true;
    auto integrate = [](double rough, double metal, double nv, int steps) {
        glm::dvec3 sum(0);
        auto v = glm::dvec3(std::sqrt(1 - nv * nv), 0, nv);
        const double pi = std::acos(-1.);
        for (int y = 0; y < steps; ++y)
        {
            double mu = (y + .5) / steps, radius = std::sqrt(1 - mu * mu);
            for (int x = 0; x < steps * 2; ++x)
            {
                double phi = (x + .5) * pi / steps;
                sum += OracleBRDF({1, 1, 1}, metal, rough, {0, 0, 1}, v,
                                  {radius * std::cos(phi), radius * std::sin(phi), mu});
            }
        }
        return sum * (pi / (steps * steps));
    };
    for (double rough : {.5, 1.})
        for (double metal : {0., 1.})
            for (double nv : {.5, 1.})
            {
                auto a = integrate(rough, metal, nv, 64), b = integrate(rough, metal, nv, 128);
                Check(glm::length(a - b) < .002 && b.x >= 0 && b.x <= 1.02,
                      "Independent direct BRDF hemisphere convergence / energy");
                if (rough == 1 && metal == 1 && nv == 1)
                    Check(std::abs(b.x - (1 - std::log(2.))) < .00002,
                          "Single-scattering rough conductor analytic loss");
            }
    std::cout << "PASS: independent FP64 hemisphere quadrature convergence, ordinary white-furnace "
                 "energy and analytic 1-ln(2) rough-conductor loss (no IBL implementation)\n";
}
static void DirectPBRChecks()
{
    DirectEnergyChecks();
    Fixture fixture;
    NativeStarter::Generate(fixture.Directory);
    auto assets = CreateRef<ProjectAssets>(fixture.Directory);
    auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    Check(bool(mesh.Accepted), "Native cooked starter did not decode");
    FramebufferSpecification spec;
    spec.Width = spec.Height = 128;
    spec.Attachments = {FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto display = Framebuffer::Create(spec);
    SceneRenderView view;
    SceneSubmission frame;
    frame.Assets = assets;
    frame.Presentation.SceneReferred = true;
    frame.Presentation.Background = {0, 0, 0, 1};
    frame.Camera.Orthographic = true;
    frame.Camera.Position = {0, 0, 3};
    frame.Camera.ViewProjection = glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 10.f) *
                                  glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0), glm::vec3(0, 1, 0));
    auto light = glm::normalize(glm::vec3(.3f, .4f, 1));
    frame.Light = SubmittedDirectional{light, glm::vec3(2)};
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.18f, .4f, .7f, 1};
    SubmittedMesh draw;
    draw.Geometry = mesh.Accepted;
    draw.World = glm::mat4(1);
    draw.Picking = 123;
    draw.Materials = {material};
    frame.Meshes = {draw};
    auto render = [&](const std::function<void()> &ui = [] {}) {
        for (auto &mesh : frame.Meshes)
            mesh.SetWorld(mesh.World);
        display->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        display->ClearAttachment(1, -1);
        SceneRenderer::Render(frame, view, display, [] {}, ui);
        Check(glGetError() == GL_NO_ERROR, "Direct PBR GL error");
    };
    for (float metallic : {0.f, .5f, 1.f})
        for (float roughness : {0.f, .1f, .25f, .5f, .75f, 1.f})
        {
            material->Metallic = metallic;
            material->Roughness = roughness;
            render();
            auto expected = OracleBRDF(glm::dvec3(material->BaseColor), metallic, roughness,
                                       {0, 0, 1}, {0, 0, 1}, glm::dvec3(light)) *
                            2.;
            NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), expected, .002,
                    "GGX direct FP64 oracle");
            NearRGB(display->ReadColorPixel(0, 64, 64), OracleNeutral(expected), .001,
                    "Neutral oracle");
            Check(display->ReadPixel(1, 64, 64) == 123, "Presented mesh integer picking lost");
        }
    material->Metallic = .5f;
    material->Roughness = .5f;
    for (double nv : {.00001, .05, .25, .5})
    {
        frame.Camera.ToCamera = glm::vec3(std::sqrt(1 - nv * nv), 0, nv);
        render();
        NearRGB(view.Radiance->ReadColorPixel(0, 64, 64),
                OracleBRDF(glm::dvec3(material->BaseColor), .5, .5, {0, 0, 1},
                           glm::dvec3(frame.Camera.ToCamera), glm::dvec3(light)) *
                    2.,
                .002, "Finite grazing direct BRDF");
    }
    frame.Camera.ToCamera = {0, 0, 1};
    material->Metallic = 1;
    material->Roughness = 0;
    frame.Light = SubmittedDirectional{{0, 0, 1}, glm::vec3(1000)};
    render();
    auto highlight = view.Radiance->ReadColorPixel(0, 64, 64);
    Check(highlight.b > 65504 && std::isfinite(highlight.b),
          "Bright floor-roughness highlight requires finite FP32 radiance");
    frame.Light = SubmittedDirectional{light, glm::vec3(2)};
    material->Metallic = 0;
    material->Roughness = .5f;
    render();
    auto radiance = view.Radiance->ReadColorPixel(0, 64, 64);
    frame.Light->Incident *= 2;
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), glm::dvec3(radiance) * 2., .002,
            "Incident light scaling");
    frame.Light.reset();
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), {}, 1e-7,
            "No light must not synthesize ambient");
    material->Model = MaterialModel::Unlit;
    material->BaseColor = {.18f, .4f, .7f, 1};
    for (float ev : {-2.f, 0.f, 2.f})
    {
        frame.Presentation.Exposure = ev;
        render();
        NearRGB(display->ReadColorPixel(0, 64, 64),
                OracleNeutral(glm::dvec3(material->BaseColor) * double(std::exp2(ev))), .001,
                "Unlit world exposure/tone");
    }
    frame.Presentation.Exposure = 0;
    render();
    auto covered = display->ReadColorPixel(0, 64, 64);
    material->Alpha = MaterialAlpha::Mask;
    material->BaseColor.a = .1f;
    render();
    Check(display->ReadPixel(1, 64, 64) == -1, "Masked fragments wrote depth/picking");
    material->BaseColor.a = 1;
    render();
    Check(display->ReadPixel(1, 64, 64) == 123, "Mask accepted coverage missing");
    for (auto scale : {glm::vec3(-1, 1, 1), glm::vec3(1, .5f, 2)})
    {
        frame.Meshes[0].World = glm::scale(glm::mat4(1), scale);
        render();
        Check(display->ReadPixel(1, 64, 64) == 123,
              "Negative/nonuniform transform winding missing");
        NearRGB(display->ReadColorPixel(0, 64, 64), glm::dvec3(covered), .001,
                "Transformed unlit color changed");
    }
    frame.Meshes[0].World = glm::mat4(1);
    // A data normal texel is decoded as UNORM, never sRGB. Top-left UV has -Y bitangent on the cube
    // front.
    std::string tga(18, '\0');
    tga[2] = 2;
    tga[12] = tga[14] = 1;
    tga[16] = 32;
    tga[17] = 0x28;
    tga += char(255);
    tga += char(255);
    tga += char(128);
    tga += char(255);
    FileSystem::WriteNewFile(fixture.Directory / "Normal.tga", tga);
    material->Model = MaterialModel::MetallicRoughness;
    material->Textures[2].Image = "Normal.tga";
    frame.Presentation.Exposure = 0;
    frame.Light = SubmittedDirectional{glm::normalize(glm::vec3(.2f, -.4f, 1)), glm::vec3(2)};
    for (float mirror : {1.f, -1.f})
    {
        frame.Meshes[0].World = glm::scale(glm::mat4(1), {mirror, .5f, 2});
        render();
        auto n = glm::normalize(glm::dvec3(mirror * (2. * 128 / 255 - 1), -1, 1));
        auto expected = OracleBRDF(glm::dvec3(material->BaseColor), 0, .5, n, {0, 0, 1},
                                   glm::dvec3(frame.Light->ToLight)) *
                        2.;
        NearRGB(view.Radiance->ReadColorPixel(0, 64, 64), expected, .002,
                "Data normal TBN / mirrored handedness");
    }
    material->Textures[2].Image.clear();
    frame.Meshes[0].World = glm::mat4(1);
    // A single reversed plane exposes double-sided back normal reversal; a closed cube cannot.
    auto plane = CreateRef<MeshAsset>(*mesh.Accepted);
    plane->Definition.ID = plane->Geometry.Mesh = 30;
    plane->Generation = 31;
    plane->Geometry.Vertices.resize(4);
    plane->Geometry.Indices.resize(6);
    plane->Geometry.Primitives[0].IndexCount = 6;
    plane->Geometry.UpdateBounds();
    frame.Meshes[0].Geometry = plane;
    frame.Meshes[0].World = glm::rotate(glm::mat4(1), float(std::acos(-1.)), glm::vec3(0, 1, 0));
    material->DoubleSided = false;
    render();
    Check(display->ReadPixel(1, 64, 64) == -1, "Single-sided backface was drawn");
    material->DoubleSided = true;
    render();
    Check(display->ReadPixel(1, 64, 64) == 123, "Double-sided backface missing");
    NearRGB(view.Radiance->ReadColorPixel(0, 64, 64),
            OracleBRDF(glm::dvec3(material->BaseColor), 0, .5, {0, 0, 1}, {0, 0, 1},
                       glm::dvec3(frame.Light->ToLight)) *
                2.,
            .002, "Double-sided normal reversal");
    frame.Meshes[0].Geometry = mesh.Accepted;
    frame.Meshes[0].World = glm::mat4(1);
    material->DoubleSided = false;
    material->Model = MaterialModel::Unlit;
    // Tilted authored normals distinguish inverse transpose from the model matrix.
    auto tilted = CreateRef<MeshAsset>(*plane);
    tilted->Definition.ID = tilted->Geometry.Mesh = 40;
    tilted->Generation = 41;
    const glm::dvec3 authoredNormal(.3, .4, std::sqrt(.75));
    const auto authoredTangent = glm::normalize(glm::dvec3(authoredNormal.z, 0, -authoredNormal.x));
    for (auto &vertex : tilted->Geometry.Vertices)
    {
        vertex.Normal = glm::vec3(authoredNormal);
        vertex.Tangent = glm::vec4(glm::vec3(authoredTangent), -1);
        vertex.Color = {.5f, .25f, .125f, .8f};
    }
    frame.Meshes[0].Geometry = tilted;
    material->Model = MaterialModel::MetallicRoughness;
    material->Textures[2].Image = "Normal.tga";
    const auto rotation = glm::rotate(glm::mat4(1), .25f, glm::vec3(0, 1, 0));
    for (double sx : {.7, -.7})
    {
        frame.Meshes[0].World = rotation * glm::scale(glm::mat4(1), glm::vec3(sx, 1.3, 2));
        render();
        auto rotate = [](glm::dvec3 v) {
            const double c = std::cos(.25), s = std::sin(.25);
            return glm::dvec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
        };
        auto n = glm::normalize(
            rotate({authoredNormal.x / sx, authoredNormal.y / 1.3, authoredNormal.z / 2}));
        auto tangent =
            rotate({authoredTangent.x * sx, authoredTangent.y * 1.3, authoredTangent.z * 2});
        tangent = glm::normalize(tangent - n * glm::dot(n, tangent));
        auto bitangent = glm::cross(n, tangent) * (sx < 0 ? 1. : -1.);
        auto mapped = glm::normalize(tangent * (2. * 128 / 255 - 1) + bitangent + n);
        auto base = glm::dvec3(material->BaseColor) * glm::dvec3(.5, .25, .125);
        NearRGB(view.Radiance->ReadColorPixel(0, 64, 64),
                OracleBRDF(base, 0, .5, mapped, {0, 0, 1}, glm::dvec3(frame.Light->ToLight)) * 2.,
                .002, "Independent tilted normal / nonuniform signed TBN / linear COLOR0");
    }
    frame.Meshes[0].Geometry = mesh.Accepted;
    frame.Meshes[0].World = glm::mat4(1);
    material->Textures[2].Image.clear();
    material->Model = MaterialModel::Unlit;
    // Independent primitive ranges must change both indexed offset and material.
    auto ranges = CreateRef<MeshAsset>(*mesh.Accepted);
    ranges->Definition.ID = ranges->Geometry.Mesh = 50;
    ranges->Generation = 51;
    ranges->Geometry.Vertices.clear();
    ranges->Geometry.Indices.clear();
    ranges->Geometry.Primitives.clear();
    for (int side = 0; side < 2; ++side)
    {
        for (int i = 0; i < 4; ++i)
        {
            auto vertex = mesh.Accepted->Geometry.Vertices[i];
            vertex.Position.x = vertex.Position.x * .8f + (side ? .5f : -.5f);
            vertex.Position.y *= .8f;
            ranges->Geometry.Vertices.push_back(vertex);
        }
        for (auto index : {0, 2, 1, 0, 3, 2})
            ranges->Geometry.Indices.push_back(side * 4 + index);
        ranges->Geometry.Primitives.push_back(
            {uint64_t(side ? 17 : 7), uint32_t(side * 6), 6, 1, {}});
    }
    ranges->Geometry.UpdateBounds();
    ranges->Definition.Slots.push_back(
        {17, "Independent second slot", "Native/Direct_v1/Surface.hmat"});
    auto red = CreateRef<MaterialDefinition>(), blue = CreateRef<MaterialDefinition>();
    red->Model = blue->Model = MaterialModel::Unlit;
    red->BaseColor = {.5f, 0, 0, 1};
    blue->BaseColor = {0, 0, .5f, 1};
    frame.Meshes[0].Geometry = ranges;
    frame.Meshes[0].Materials = {red, blue};
    render();
    NearRGB(view.Radiance->ReadColorPixel(0, 32, 64), {.5, 0, 0}, 1e-6,
            "First material primitive range");
    NearRGB(view.Radiance->ReadColorPixel(0, 96, 64), {0, 0, .5}, 1e-6,
            "Nonzero indexed offset / second material primitive range");
    Check(display->ReadPixel(1, 32, 64) == 123 && display->ReadPixel(1, 96, 64) == 123,
          "Primitive ranges lost shared entity identity");
    frame.Meshes[0].Geometry = mesh.Accepted;
    frame.Meshes[0].Materials = {material};
    // Real indexed depth: mask discard reveals a separately identified rear instance.
    auto rearMaterial = CreateRef<MaterialDefinition>();
    rearMaterial->Model = MaterialModel::Unlit;
    rearMaterial->BaseColor = {0, 0, 1, 1};
    auto rear = frame.Meshes.front();
    rear.Picking = 777;
    rear.World = glm::translate(glm::mat4(1), glm::vec3(0, 0, -2));
    rear.Materials = {rearMaterial};
    frame.Meshes.push_back(rear);
    render();
    Check(display->ReadPixel(1, 64, 64) == 123, "Depth selected rear opaque geometry");
    material->Alpha = MaterialAlpha::Mask;
    material->BaseColor.a = .1f;
    render();
    Check(display->ReadPixel(1, 64, 64) == 777,
          "Mask wrote depth instead of revealing rear geometry");
    material->BaseColor.a = 1;
    material->Alpha = MaterialAlpha::Opaque;
    frame.Meshes.pop_back();
    // Vertex UV and color texture interpretation use the normal asset cache.
    std::string asymmetric(18, '\0');
    asymmetric[2] = 2;
    asymmetric[12] = asymmetric[14] = 2;
    asymmetric[16] = 32;
    asymmetric[17] = 0x28;
    for (auto rgba : {std::array<unsigned char, 4>{128, 0, 0, 255},
                      {0, 128, 0, 255},
                      {0, 0, 128, 255},
                      {128, 128, 128, 255}})
    {
        asymmetric += char(rgba[2]);
        asymmetric += char(rgba[1]);
        asymmetric += char(rgba[0]);
        asymmetric += char(rgba[3]);
    }
    FileSystem::WriteNewFile(fixture.Directory / "Asymmetric.tga", asymmetric);
    material->BaseColor = glm::vec4(1);
    material->Textures[0].Image = "Asymmetric.tga";
    material->Textures[0].Min = material->Textures[0].Mag = TextureFilter::Nearest;
    frame.Presentation.Exposure = 0;
    render();
    auto topLeft = view.Radiance->ReadColorPixel(0, 48, 80),
         bottomLeft = view.Radiance->ReadColorPixel(0, 48, 48),
         topRight = view.Radiance->ReadColorPixel(0, 80, 80);
    Check(SRGB8ReadbackMatches(topLeft.r, 128) && topLeft.g == 0 && topLeft.b == 0 &&
              SRGB8ReadbackMatches(bottomLeft.b, 128) && bottomLeft.r == 0 &&
              SRGB8ReadbackMatches(topRight.g, 128),
          "Indexed material texture UV / sRGB sampling mismatch");
    material->Textures[0].Image.clear();
    material->BaseColor = {.18f, .4f, .7f, 1};
    // UI runs after presentation, with independent linear color and entity IDs.
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
        render(ui);
        NearRGB(display->ReadColorPixel(0, 64, 64), {.25, .5, .75}, .001,
                "UI brightness coupled to world exposure");
        Check(display->ReadPixel(1, 64, 64) == 456,
              "UI integer picking missing after presentation");
    }
    material->Model = MaterialModel::MetallicRoughness;
    frame.Light = SubmittedDirectional{{0, 0, 1}, glm::vec3(1)};
    for (auto inspect : {SurfaceView::BaseColor, SurfaceView::Normal, SurfaceView::Tangent,
                         SurfaceView::MetalRoughAO})
    {
        frame.Debug = inspect;
        render();
        Check(!view.Radiance, "Display inspection allocated a lighting target");
        glm::dvec3 expected = inspect == SurfaceView::BaseColor ? glm::dvec3(material->BaseColor)
                              : inspect == SurfaceView::Normal  ? glm::dvec3(.5, .5, 1)
                              : inspect == SurfaceView::Tangent ? glm::dvec3(1, .5, .5)
                                                                : glm::dvec3(0, .5, 1);
        NearRGB(display->ReadColorPixel(0, 64, 64), expected, .001,
                "Surface inspection decoded wrong semantics or applied tone mapping");
    }
    frame.Debug = SurfaceView::Shaded;
    // A hostile caller state must survive successful and failing submissions.
    glEnable(GL_SCISSOR_TEST);
    glScissor(5, 7, 11, 13);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_GEQUAL);
    glFrontFace(GL_CW);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glActiveTexture(GL_TEXTURE4);
    glEnablei(GL_BLEND, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    GLint beforeProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &beforeProgram);
    auto checkState = [&] {
        GLint value;
        Check(glIsEnabledi(GL_BLEND, 1), "Integer blend enable not restored");
        glGetIntegerv(GL_READ_BUFFER, &value);
        Check(value == GL_COLOR_ATTACHMENT1, "Read attachment not restored");
        glGetIntegerv(GL_DRAW_BUFFER1, &value);
        Check(value == GL_NONE, "Draw attachments not restored");
        glGetIntegerv(GL_DEPTH_FUNC, &value);
        Check(value == GL_GEQUAL && !RenderCommand::GetRasterState().DepthWrite,
              "Depth state not restored");
        glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
        Check(value == GL_TEXTURE4, "Texture unit not restored");
        glGetIntegerv(GL_CURRENT_PROGRAM, &value);
        Check(value == beforeProgram, "Program not restored");
        Check(RenderCommand::GetRasterState().ClipRect == glm::ivec4(5, 7, 11, 13),
              "Scissor not restored");
    };
    SceneRenderer::Render(frame, view, display, [] {}, [] {});
    checkState();
    bool failed = false;
    try
    {
        SceneRenderer::Render(frame, view, {}, [] {}, [] {});
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    Check(failed, "HDR missing host target silently accepted");
    checkState();
    for (bool duringWorld : {false, true})
    {
        failed = false;
        try
        {
            SceneRenderer::Render(
                frame, view, display,
                [&] {
                    if (duringWorld)
                        throw std::runtime_error("world producer failure");
                },
                [&] {
                    if (!duringWorld)
                        throw std::runtime_error("screen producer failure");
                });
        }
        catch (const std::exception &)
        {
            failed = true;
        }
        Check(failed, "Injected render producer failure was ignored");
        checkState();
    }
    SceneRenderer::Render(frame, view, display, [] {}, [] {});
    checkState(); // Retry retains a valid binding owner.

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    display->Unbind();
    for (auto v : {glm::dvec3(0, 0, 1), glm::normalize(glm::dvec3(.8, 0, .2))})
    {
        auto l = glm::normalize(glm::dvec3(.2, .3, 1));
        auto a = OracleBRDF({.2, .4, .7}, .5, .5, {0, 0, 1}, v, l) / l.z;
        auto b = OracleBRDF({.2, .4, .7}, .5, .5, {0, 0, 1}, l, v) / v.z;
        Check(glm::length(a - b) < 1e-10, "Independent BRDF reciprocity");
    }
    std::cout << "PASS: Direct_v1 FP64/GPU roughness-metal grid, finite FP32 highlights, light "
                 "scaling/no ambient, Neutral/exposure/UI independence, masks, transforms, picking "
                 "and exception state restoration\n";
}
