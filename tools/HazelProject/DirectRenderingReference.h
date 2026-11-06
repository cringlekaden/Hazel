#pragma once
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Assets/ImportedAsset.h"
#include "Hazel/Assets/NativeStarter.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Scene/Prefab.h"
#include <cmath>
// Maintainer native CPU authoring recipe. Existing cases are never overwritten.
inline void AddDirectRenderingReference(const std::filesystem::path &descriptor,
                                        const std::filesystem::path &templates)
{
    using namespace Hazel;
    auto project = Project::LoadCandidate(descriptor);
    if (!project)
        throw std::runtime_error("Open a valid Reference project");
    auto root = project->GetAssetRoot();
    const std::filesystem::path folder = "Reference/Direct_v1";
    if (std::filesystem::exists(root / folder))
        throw std::runtime_error(
            "Direct_v1 already exists; frozen conditions cannot be overwritten");
    NativeStarter::Ensure(templates, root);
    std::filesystem::create_directories(root / folder / "geometry");
    auto put = [&](const auto &path, const std::string &data) {
        std::filesystem::create_directories((root / path).parent_path());
        FileSystem::WriteNewFile(root / path, data);
    };
    GeometryData sphere;
    sphere.Mesh = 11;
    const int rows = 24, columns = 48;
    const float pi = std::acos(-1.f);
    for (int y = 0; y <= rows; ++y)
        for (int x = 0; x <= columns; ++x)
        {
            float theta = pi * y / rows, phi = 2 * pi * x / columns;
            MeshVertex v;
            v.Normal = {std::sin(theta) * std::cos(phi), std::cos(theta),
                        std::sin(theta) * std::sin(phi)};
            v.Position = v.Normal * .5f;
            v.Tangent = {-std::sin(phi), 0, std::cos(phi), 1};
            v.UV = {float(x) / columns, float(y) / rows};
            sphere.Vertices.push_back(v);
        }
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < columns; ++x)
        {
            uint32_t a = y * (columns + 1) + x, b = a + 1, c = a + columns + 2, d = a + columns + 1;
            if (y > 0)
                for (auto i : {a, b, c})
                    sphere.Indices.push_back(i);
            if (y < rows - 1)
                for (auto i : {a, c, d})
                    sphere.Indices.push_back(i);
        }
    sphere.Primitives.push_back({14, 0, uint32_t(sphere.Indices.size()), 1, {}});
    sphere.UpdateBounds();
    auto bytes = EncodeGeometry(sphere);
    MaterialDefinition neutral;
    neutral.Name = "Direct_v1 neutral dielectric";
    neutral.BaseColor = {.18f, .18f, .18f, 1};
    auto material = WriteMaterialText(neutral);
    put(folder / "Neutral.hmat", material);
    MeshDefinition definition{11,
                              "Direct_v1 UV sphere",
                              folder / "Sphere.himport",
                              {{14, "Surface", folder / "Neutral.hmat"}}};
    put(folder / "Sphere.hmesh", WriteMeshText(definition));
    put(folder / "geometry/Sphere.hgeom", bytes);
    auto nodes = CreateRef<Scene>();
    auto parent = nodes->CreateEntityWithUUID(12, "Direct_v1 sphere");
    auto node = nodes->CreateEntityWithUUID(16, "Sphere");
    nodes->Reparent(node, parent, TransformPolicy::KeepLocal);
    node.AddComponent<MeshRendererComponent>().Mesh = folder / "Sphere.hmesh";
    auto prefab = Prefab::Serialize(root, nodes, parent, true);
    put(folder / "Sphere.hprefab", prefab);
    ImportManifest manifest;
    manifest.ID = 10;
    manifest.Generation = 13;
    manifest.PrefabRoot = 12;
    manifest.Source = folder / "Source.generated";
    std::string source =
        "Hazel original Direct_v1 sphere: radius .5 m, 24 latitude x 48 longitude, +Y up, top-left "
        "UV, analytic normals and tangent; excludes pole degenerates";
    manifest.SourceHash = AssetHash(source);
    manifest.Producer = "Hazel.Native.Direct_v1";
    manifest.SourceNotice =
        "Original Hazel diagnostic geometry and materials; Apache-2.0 (repository LICENSE)";
    manifest.Prefab = folder / "Sphere.hprefab";
    manifest.PrefabSource = AssetHash("Direct_v1 sphere node");
    manifest.PrefabAcceptedHash = AssetHash(prefab);
    manifest.Nodes = {{16, AssetHash("sphere node"), "native/sphere"}};
    manifest.Materials = {
        {15, AssetHash(material), folder / "Neutral.hmat", AssetHash(material), "native/neutral"}};
    ImportMesh output;
    output.ID = 11;
    output.Source = AssetHash(bytes);
    output.SourceKey = "native/sphere";
    output.Output = folder / "Sphere.hmesh";
    output.Geometry = folder / "geometry/Sphere.hgeom";
    output.GeometryHash = AssetHash(bytes);
    output.Slots = {{14, AssetHash("sphere surface"), folder / "Neutral.hmat", "native/surface"}};
    manifest.Meshes = {output};
    put(manifest.Source, source);
    put(folder / "Sphere.himport", WriteImportText(manifest));
    AuditImport(root, folder / "Sphere.himport");
    std::string outputs =
        "{\n  \"Version\": 1, \"Case\": \"Direct_v1\", \"BRDFRecipe\": "
        "\"GGX_SmithCorrelated_Schlick_Lambert_SingleScattering_v1\",\n  \"RoughnessFloor\": "
        "0.045, \"Resolution\": [1280,720], \"Exposure\": 0, \"Tone\": \"KhronosPBRNeutral\", "
        "\"MSAA\": 1, \"Environment\": \"None\",\n  \"Cameras\": "
        "{\"Position\":[0,0,10],\"Near\":0.1,\"Far\":100,\"PerspectiveFOVDegrees\":45,"
        "\"OrthographicSize\":10},\n  \"DirectionalTravelEuler\":[-0.45,0.6,0], "
        "\"DirectionalRGB\":[1,1,1], \"Intensity\":3,\n  \"Scenes\": {\n";
    for (int mode = 0; mode < 3; ++mode)
    {
        bool unlit = mode == 2, ortho = mode == 1;
        std::string name = unlit   ? "PerspectiveUnlit"
                           : ortho ? "OrthographicPBR"
                                   : "PerspectivePBR";
        auto scene = CreateRef<Scene>();
        scene->SetName("Direct_v1 / " + name);
        scene->SetAssets(CreateRef<ProjectAssets>(root));
        scene->Presentation().SceneReferred = !unlit;
        scene->Presentation().Background = {.01f, .01f, .01f, 1};
        scene->OnViewportResize(1280, 720);
        auto camera = scene->CreateEntityWithUUID(1, "Frozen " + name + " camera");
        camera.GetComponent<TransformComponent>().Translation = {0, 0, 10};
        auto &c = camera.AddComponent<CameraComponent>().Camera;
        if (ortho)
            c.SetOrthographic(10, .1f, 100);
        else
            c.SetPerspective(glm::radians(45.f), .1f, 100);
        auto light = scene->CreateEntityWithUUID(2, "Direct only key / no environment");
        light.GetComponent<TransformComponent>().Rotation = {-.45f, .6f, 0};
        light.AddComponent<DirectionalLightComponent>().Intensity = 3;
        const float roughness[] = {0, .1f, .25f, .5f, .75f, 1};
        const float metallic[] = {0, .5f, 1};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 6; ++col)
            {
                std::string key =
                    std::to_string(row) + "_" + std::to_string(col) + (unlit ? "_unlit" : "");
                MaterialDefinition mat;
                mat.Name =
                    "r=" + std::to_string(roughness[col]) + " m=" + std::to_string(metallic[row]);
                mat.BaseColor = {.5f, .18f, .055f, 1};
                mat.Roughness = roughness[col];
                mat.Metallic = metallic[row];
                if (unlit)
                    mat.Model = MaterialModel::Unlit;
                auto path = folder / "materials" / (key + ".hmat");
                if (!std::filesystem::exists(root / path))
                    put(path, WriteMaterialText(mat));
                auto instance = scene->CreateEntityWithUUID(10 + row * 6 + col, mat.Name);
                instance.GetComponent<TransformComponent>().Translation = {-3.75f + 1.5f * col,
                                                                           1.7f - 1.5f * row, 0};
                auto &mesh = instance.AddComponent<MeshRendererComponent>();
                mesh.Mesh = folder / "Sphere.hmesh";
                mesh.Materials[14] = path;
            }
        auto cube = scene->CreateEntityWithUUID(50, "Negative nonuniform cube");
        cube.GetComponent<TransformComponent>().Translation = {5, 0, 0};
        cube.GetComponent<TransformComponent>().Scale = {-.8f, 1.5f, .8f};
        auto &mesh = cube.AddComponent<MeshRendererComponent>();
        mesh.Mesh = "Native/Direct_v1/UnitCube.hmesh";
        if (unlit)
            mesh.Materials[7] = folder / "materials/0_0_unlit.hmat";
        auto text = scene->CreateEntityWithUUID(60, "World text");
        text.GetComponent<TransformComponent>().Translation = {-4.5f, 3, 0};
        text.GetComponent<TransformComponent>().Scale = {.35f, .35f, 1};
        text.AddComponent<TextComponent>().TextString =
            "Direct_v1 / " + name + " / no ambient or IBL";
        auto sprite = scene->CreateEntityWithUUID(61, "World sprite");
        sprite.GetComponent<TransformComponent>().Translation = {-5, -2.8f, 0};
        sprite.AddComponent<SpriteRendererComponent>().Color = {.21404114f, .5f, 1, .5f};
        auto circle = scene->CreateEntityWithUUID(62, "World circle");
        circle.GetComponent<TransformComponent>().Translation = {-3.8f, -2.8f, 0};
        circle.AddComponent<CircleRendererComponent>().Thickness = .2f;
        auto canvas = scene->CreateEntityWithUUID(70, "Exposure independent UI");
        canvas.AddComponent<UICanvasComponent>();
        auto label = scene->CreateEntityWithUUID(71, "UI case label");
        scene->Reparent(label, canvas, TransformPolicy::KeepLocal);
        auto &rect = label.AddComponent<UIRectComponent>();
        rect.Offset = {24, 660};
        rect.FixedSize = {1200, 40};
        label.AddComponent<TextComponent>().TextString =
            "Direct_v1 / roughness 0 .1 .25 .5 .75 1 / metallic 0 .5 1 / UI unaffected by exposure";
        label.AddComponent<UITextStyleComponent>().Size = 20;
        auto scenePath = std::filesystem::path("Scenes") / ("Direct_v1-" + name + ".hazel");
        auto textBytes = SceneSerializer(scene, root).SerializeText();
        put(scenePath, textBytes);
        outputs += "    \"" + scenePath.generic_u8string() + "\": \"" + AssetHash(textBytes) +
                   "\"" + (mode < 2 ? ",\n" : "\n");
        auto config = CreateRef<Project>();
        config->GetConfig() = project->GetConfig();
        config->GetConfig().StartScene = scenePath;
        config->GetConfig().SceneDefaults = SceneDefaults::DirectDefaults();
        FileSystem::WriteNewFile(descriptor.parent_path() /
                                     ("RenderingReference-Direct-" + name + ".hproj"),
                                 ProjectSerializer(config).SerializeText());
    }
    put(folder / "outputs.json", outputs + "  }\n}\n");
    put(folder / "NOTICE.txt",
        manifest.SourceNotice +
            "\nGenerated diagnostic geometry; no external model, lighting data or reference "
            "render. Source metadata records the exact construction.\n");
}

inline void AddDirectSurfaceReference(const std::filesystem::path &descriptor)
{
    using namespace Hazel;
    auto project = Project::LoadCandidate(descriptor);
    if (!project)
        throw std::runtime_error("Open the Reference project");
    auto root = project->GetAssetRoot();
    const std::filesystem::path folder = "Reference/Direct_v1/Surfaces";
    if (std::filesystem::exists(root / folder))
        throw std::runtime_error("Surface conditions already exist");
    std::filesystem::create_directories(root / folder);
    auto put = [&](const auto &p, const std::string &data) {
        FileSystem::WriteNewFile(root / p, data);
    };
    auto image = [&](bool normal) {
        std::string tga(18, '\0');
        tga[2] = 2;
        tga[12] = tga[14] = 8;
        tga[16] = 32;
        tga[17] = 0x28;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
            {
                unsigned char r = normal  ? 128
                                  : x < 4 ? 255
                                          : 32,
                              g = normal  ? (y < 4 ? 192 : 64)
                                  : y < 4 ? 128
                                          : 255,
                              b = normal ? 255 : 32,
                              a = normal                   ? 255
                                  : (x == y || x + y == 7) ? 0
                                                           : 255;
                tga += char(b);
                tga += char(g);
                tga += char(r);
                tga += char(a);
            }
        return tga;
    };
    put(folder / "NormalPlusY.tga", image(true));
    put(folder / "AsymmetricMask.tga", image(false));
    MaterialDefinition mapped;
    mapped.Name = "Linear +Y normal / UV seam";
    mapped.BaseColor = {.18f, .4f, .7f, 1};
    mapped.Textures[2].Image = folder / "NormalPlusY.tga";
    mapped.Textures[2].Min = mapped.Textures[2].Mag = TextureFilter::Nearest;
    put(folder / "Mapped.hmat", WriteMaterialText(mapped));
    auto mask = mapped;
    mask.Name = "Asymmetric sRGB / mask / double-sided";
    mask.Textures[0].Image = folder / "AsymmetricMask.tga";
    mask.Textures[0].Min = mask.Textures[0].Mag = TextureFilter::Nearest;
    mask.Alpha = MaterialAlpha::Mask;
    mask.DoubleSided = true;
    put(folder / "Mask.hmat", WriteMaterialText(mask));
    auto scene = CreateRef<Scene>();
    scene->SetName("Direct_v1 / SurfaceConventions");
    scene->SetAssets(CreateRef<ProjectAssets>(root));
    scene->Presentation().SceneReferred = true;
    scene->Presentation().Background = {.01f, .01f, .01f, 1};
    scene->OnViewportResize(1280, 720);
    auto camera = scene->CreateEntityWithUUID(1, "Frozen surface camera");
    camera.GetComponent<TransformComponent>().Translation = {0, 0, 10};
    camera.AddComponent<CameraComponent>().Camera.SetPerspective(glm::radians(45.f), .1f, 100);
    auto light = scene->CreateEntityWithUUID(2, "Frozen direct key");
    light.GetComponent<TransformComponent>().Rotation = {-.45f, .6f, 0};
    light.AddComponent<DirectionalLightComponent>().Intensity = 3;
    for (int i = 0; i < 6; ++i)
    {
        auto e =
            scene->CreateEntityWithUUID(10 + i, i == 0   ? "Geometric normal"
                                                : i == 1 ? "Mapped +Y"
                                                : i == 2 ? "Mirrored mapped"
                                                : i == 3 ? "Nonuniform mapped"
                                                : i == 4 ? "Masked / double sided"
                                                         : "Rear geometry through masked holes");
        auto &t = e.GetComponent<TransformComponent>();
        t.Translation = {-3.f + 3.f * (i % 3), i < 3 ? 1.5f : -1.5f, 0};
        t.Scale = glm::vec3(1.8f);
        if (i == 2)
            t.Scale.x *= -1;
        if (i == 3)
            t.Scale = {2.4f, 1.2f, .7f};
        auto &mesh = e.AddComponent<MeshRendererComponent>();
        mesh.Mesh = i < 4 ? "Reference/Direct_v1/Sphere.hmesh" : "Native/Direct_v1/UnitCube.hmesh";
        if (i > 0)
            mesh.Materials[i < 4 ? 14 : 7] = folder / (i == 4 ? "Mask.hmat" : "Mapped.hmat");
        if (i == 5)
        {
            t.Translation = {0, -1.5f, -2};
            t.Scale = glm::vec3(2.4f);
        }
    }
    // Reuse Stage-2 accepted geometry directly, without parser/source access.
    std::vector<std::filesystem::path> paths;
    for (const auto &item : std::filesystem::recursive_directory_iterator(root))
        if (item.is_regular_file())
            paths.push_back(item.path().lexically_relative(root));
    for (const auto &p : paths)
        if (p.extension() == ".hmesh" &&
            p.generic_u8string().find("Import_v1/Native/ProvidedTangents/meshes/") !=
                std::string::npos)
        {
            auto e = scene->CreateEntityWithUUID(40, "Stage-2 supplied tangents and COLOR0");
            e.GetComponent<TransformComponent>().Translation = {3, -1.5f, 0};
            e.AddComponent<MeshRendererComponent>().Mesh = p;
            break;
        }
    auto canvas = scene->CreateEntityWithUUID(70, "Screen UI");
    canvas.AddComponent<UICanvasComponent>();
    auto label = scene->CreateEntityWithUUID(71, "Surface labels");
    scene->Reparent(label, canvas, TransformPolicy::KeepLocal);
    auto &rect = label.AddComponent<UIRectComponent>();
    rect.Offset = {24, 24};
    rect.FixedSize = {1230, 80};
    label.AddComponent<TextComponent>().TextString =
        "Direct_v1 / geometric - mapped +Y - mirrored / nonuniform - asymmetric mask - Stage-2 "
        "COLOR0\nUse viewport Normal / Tangent / Metal-Rough-AO inspection; scene stays unchanged.";
    label.AddComponent<UITextStyleComponent>().Size = 20;
    auto reference = std::filesystem::path("Scenes/Direct_v1-SurfaceConventions.hazel");
    auto bytes = SceneSerializer(scene, root).SerializeText();
    put(reference, bytes);
    auto config = CreateRef<Project>();
    config->GetConfig() = project->GetConfig();
    config->GetConfig().StartScene = reference;
    config->GetConfig().SceneDefaults = SceneDefaults::DirectDefaults();
    FileSystem::WriteNewFile(descriptor.parent_path() /
                                 "RenderingReference-Direct-SurfaceConventions.hproj",
                             ProjectSerializer(config).SerializeText());
    put(folder / "outputs.json",
        std::string(
            "{\"Version\":1,\"Resolution\":[1280,720],\"Exposure\":0,\"Tone\":"
            "\"KhronosPBRNeutral\",\"CameraPosition\":[0,0,10],\"FOVDegrees\":45,\"Near\":0.1,"
            "\"Far\":100,\"DirectionalEuler\":[-0.45,0.6,0],\"Intensity\":3,\"Scene\":\"") +
            reference.generic_u8string() + "\",\"SceneHash\":\"" + AssetHash(bytes) + "\"}\n");
    put(folder / "NOTICE.txt",
        "Original Hazel 8x8 diagnostic pixels, native authored materials and scene; Apache-2.0 "
        "(Hazel LICENSE). Normal data is +Y linear UNORM; asymmetric color/mask is sRGB with "
        "unchanged alpha. Reuses native Direct_v1 and frozen Import_v1 cooked geometry.\n");
}
