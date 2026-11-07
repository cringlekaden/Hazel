#pragma once
#include "GeometricNormalReference.h"
#include "IBLSurfaceReference.h"
// Probe the production PBR program, not a reimplementation in a test shader.
// Orthographic coverage is fixed on a cube face; ToCamera supplies the desired
// local BRDF direction independently of raster coverage.
static void IBLSurfaceRenderingChecks(SceneSubmission frame, SubmittedMesh draw,
                                      const Ref<Framebuffer> &display)
{
    RenderCommand::StateScope scope;
    auto selected = FileSystem::GetEnvironmentPath("HAZEL_RENDERING_SHOWCASE_PROJECT");
    Check(!selected.empty(), "Surface regression requires the maintained Showcase");
    auto project = Project::LoadCandidate(selected);
    auto studio = CreateRef<PreparedEnvironment>(
        ReadPreparedEnvironment(project->GetAssetRoot(), "Environments/Studio.henv"));
    auto source = DecodeRadianceHDR(
        ReadAssetBytes(project->GetAssetRoot() / "Environments/Studio.hdr", 64 * 1024 * 1024));
    auto material = CreateRef<MaterialDefinition>();
    material->BaseColor = {.955f, .638f, .538f, 1};
    material->Metallic = material->Roughness = 1;
    draw.Materials = {material};
    frame.Meshes = {draw};
    frame.Light.reset();
    frame.Environment = studio;
    frame.Presentation.Environment.Asset = "Environments/Studio.henv";
    frame.Presentation.Environment.Background = false;
    frame.Presentation.Environment.Yaw = 0;
    frame.Presentation.Environment.Gain = 1;
    frame.Presentation.Exposure = 0;
    frame.Debug = SurfaceView::SpecularIBL;
    frame.LinearLightingInspection = true;
    auto spec = display->GetSpecification();
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
    auto target = Framebuffer::Create(spec);
    SceneRenderView view;
    auto render = [&](glm::dvec3 n, glm::dvec3 v)
    {
        auto N = glm::vec3(n), V = glm::vec3(v);
        auto up = std::abs(N.y) > .9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        frame.Camera = {glm::ortho(-1.f, 1.f, -1.f, 1.f, .1f, 10.f) *
                            glm::lookAt(N * 3.f, glm::vec3(0), up),
                        N * 3.f, V, true};
        frame.Meshes[0].SetWorld(glm::inverse(glm::lookAt(glm::vec3(0), -N, up)));
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        SceneRenderer::Render(frame, view, target, [] {}, [] {});
        Check(target->ReadPixel(1, spec.Width / 2, spec.Height / 2) == draw.Picking,
              "Surface probe lost mesh picking");
        return glm::dvec3(target->ReadColorPixel(0, spec.Width / 2, spec.Height / 2));
    };
    // Off-center directions and both sides of three cube seams. Quadrature
    // evaluates the original bilinear HDR field, not stored texel centers.
    std::vector<SurfaceReferenceInput> inputs;
    for (auto n : {glm::dvec3(.3, .8, -.4), glm::dvec3(-.8, -.2, .3), glm::dvec3(1, .13, .9999),
                   glm::dvec3(.9999, .13, 1), glm::dvec3(.12, 1, .9999), glm::dvec3(.12, .9999, 1),
                   glm::dvec3(-1, .9999, .23), glm::dvec3(-.9999, 1, .23)})
    {
        n = glm::normalize(n);
        auto tangent = glm::normalize(glm::cross(n, glm::dvec3(0, 0, 1)));
        auto v = glm::normalize(n * .8 + tangent * .6);
        auto normal = glm::normalize(n + v); // reflect(-V,N) is the chosen seam direction.
        inputs.push_back({normal, v, normal, glm::dvec3(material->BaseColor), 1, 1, 1});
    }
    auto coarse = IntegrateSurfaceReference(source, inputs, 1),
         fine = IntegrateSurfaceReference(source, inputs, 2);
    double maxReconstruction = 0, maxModel = 0;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        auto &s = inputs[i];
        Check(glm::length(fine[i].Prefilter - coarse[i].Prefilter) < .0001,
              "Surface source quadrature did not converge");
        auto ab = IBLReferenceIntegral(glm::dot(s.N, s.V), 1);
        // Isolate angular reconstruction from the LUT's admitted quadrature /
        // finite endpoint representation. Sample the accepted CPU RG artifact
        // independently, with the GL normalized-coordinate pixel-center rule.
        const auto &lut = studio->BRDF;
        double px = glm::dot(s.N, s.V) * lut.Side - .5, py = lut.Side - .5;
        int x = int(std::floor(px)), y = int(std::floor(py));
        auto at = [&](int xx, int yy)
        {
            size_t p = (size_t(std::clamp(yy, 0, int(lut.Side) - 1)) * lut.Side +
                        std::clamp(xx, 0, int(lut.Side) - 1)) *
                       2;
            return glm::dvec2(lut.Mips[0][p], lut.Mips[0][p + 1]);
        };
        auto sampledAB = glm::mix(glm::mix(at(x, y), at(x + 1, y), px - x),
                                  glm::mix(at(x, y + 1), at(x + 1, y + 1), px - x), py - y);
        Check(glm::length(sampledAB - ab) < .01,
              "Accepted LUT exceeds the independent BRDF integration bound");
        auto expected = fine[i].Prefilter * (s.Base * sampledAB.x + sampledAB.y);
        auto actual = render(s.N, s.V);
        double error = glm::length(actual - expected) / std::max(.02, glm::length(expected));
        std::cout << "Surface endpoint " << i << " expected " << expected.x << "," << expected.y
                  << "," << expected.z << " actual " << actual.x << "," << actual.y << ","
                  << actual.z << " relative " << error << '\n';
        Check(error < .006,
              "Rough endpoint reconstruction is coarse or discontinuous at a cube seam");
        maxReconstruction = std::max(maxReconstruction, error);
        maxModel = std::max(maxModel, glm::length(actual - fine[i].Spec));
    }
    // A geometrically back-facing view still has no reflection. Visible
    // geometry with a back-facing shading normal is a different case below.
    for (float metal : {0.f, 1.f})
        for (float rough : {.2f, .5f, 1.f})
        {
            material->Metallic = metal;
            material->Roughness = rough;
            auto actual = render({0, 0, 1}, glm::normalize(glm::dvec3(1, 0, -.02)));
            Check(glm::length(actual) < 1e-7,
                  "Geometrically back-facing surface manufactured reflection");
            actual = render({0, 0, 1}, glm::normalize(glm::dvec3(1, 0, .02)));
            Check(glm::length(actual) > .001, "Valid grazing Fresnel was disabled");
        }
    auto constant = CreateRef<PreparedEnvironment>();
    constant->Identity = "geometric-normal-constant";
    constant->Generation = UUID();
    constant->BRDF = studio->BRDF;
    unsigned kind = 0;
    for (auto image : {&constant->Background, &constant->Irradiance, &constant->Specular})
    {
        image->Kind = LightingImageKind(++kind);
        image->Side = 1;
        image->Mips = {std::vector<float>(18, kind == 2 ? float(std::acos(-1.) * .25) : .25f)};
    }
    frame.Environment = constant;
    material->BaseColor = {.18f, .4f, .7f, 1};
    auto plane = CreateRef<MeshAsset>(*draw.Geometry);
    plane->Definition.ID = plane->Geometry.Mesh = UUID();
    plane->Geometry.Vertices.resize(4);
    plane->Geometry.Indices.resize(6);
    plane->Geometry.Primitives[0].IndexCount = 6;
    plane->Geometry.UpdateBounds();
    for (auto raw : {glm::dvec3(0, 0, 1), glm::dvec3(-.1, 0, 1), glm::dvec3(-1, .2, .05),
                     glm::dvec3(.2, .2, 1), glm::dvec3(.7, -.2, .5), glm::dvec3(-.2, 1, -.1)})
    {
        raw = glm::normalize(raw);
        plane->Generation = UUID();
        auto tangent = glm::normalize(
            glm::cross(std::abs(raw.z) > .9 ? glm::dvec3(0, 1, 0) : glm::dvec3(0, 0, 1), raw));
        for (auto &vertex : plane->Geometry.Vertices)
        {
            vertex.Normal = glm::vec3(raw);
            vertex.Tangent = glm::vec4(glm::vec3(tangent), 1);
        }
        frame.Meshes[0].Geometry = plane;
        for (auto v : {glm::normalize(glm::dvec3(1, 0, .02)), glm::normalize(glm::dvec3(1, .1, .3)),
                       glm::dvec3(0, 0, 1)})
        {
            auto expected = GeometricNormalReference(raw, {0, 0, 1}, v);
            frame.Debug = SurfaceView::Normal;
            frame.LinearLightingInspection = false;
            auto actual = render({0, 0, 1}, v) * 2. - 1.;
            Check(glm::length(actual - expected) < 2e-5,
                  "Shading normal differs from independent geometric-horizon constraint");
            Check(glm::dot(actual, v) > 0 && glm::reflect(-v, actual).z >= -1e-5,
                  "Resolved normal points the viewer/reflection beneath visible geometry");
            for (float rough : {.2f, 1.f})
            {
                auto ab = IBLReferenceIntegral(glm::dot(expected, v), rough);
                material->Roughness = rough;
                for (float metal : {0.f, 1.f})
                {
                    material->Metallic = metal;
                    frame.Debug = SurfaceView::SpecularIBL;
                    frame.LinearLightingInspection = true;
                    auto f0 =
                        glm::mix(glm::dvec3(.04), glm::dvec3(material->BaseColor), double(metal));
                    NearRGB(
                        glm::vec4(render({0, 0, 1}, v), 1), .25 * (f0 * ab.x + ab.y), .003,
                        "Geometric-normal repair lost Fresnel or changed constant-furnace energy");
                }
            }
        }
    }
    Check(glGetError() == GL_NO_ERROR, "Surface reconstruction GL error");
    target->Unbind();
    std::cout
        << "PASS: production rough endpoint FP64 off-center/seam "
           "reconstruction, max relative "
        << maxReconstruction << "; full-BRDF maximum absolute RGB length difference " << maxModel
        << " (split-sum model limit); negative/positive shading-horizon "
           "reflection and picking; independent constrained-normal direction and grazing furnace\n";
}
