#pragma once
#include "ImportedSceneData.h"
#include <cstring>
#include "ImportFixture.h"
inline void ImportCookingChecks()
{
    using namespace Hazel;
    using namespace Hazel::Import;
    auto [json, bin] = ImportFixture();
    auto bytes = ImportGLB(json, bin);
    auto data = ReadGLB(bytes, -1);
    Check(data.Meshes.size() == 1 && data.Meshes[0].Primitives.size() == 2 &&
              data.Nodes.size() == 2 && data.Nodes[0].Mesh == data.Nodes[1].Mesh &&
              data.Nodes[1].Scale.x == -1,
          "GLB lost primitives/shared mesh/signed node-local transform");
    Check(data.Materials[0].Values.Metallic == 0 && data.Materials[0].Values.Roughness == 1 &&
              data.Materials[1].Values.Model == MaterialModel::Unlit,
          "GLB material defaults/explicit unlit semantics lost");
    auto geometry = CookMesh(data.Meshes[0], data.Materials, 51, {61, 62});
    Check(geometry.Primitives[0].Slot == 61 && geometry.Primitives[1].Slot == 62 &&
              geometry.Vertices[1].Normal.z == 1,
          "Common mesh cook lost stable slots/generated flat normals");
    Check(data.Meshes[0].Primitives[0].Vertices[1].UV.x == 1,
          "Normalized unsigned UV accessor was not decoded");
    auto numericJSON = json;
    auto numericAt = numericJSON.find("\"count\":3");
    numericJSON.replace(numericAt, std::strlen("\"count\":3"), "\"count\":3.0");
    Check(ReadGLB(ImportGLB(numericJSON, bin), -1).Meshes.size() == 1,
          "Integral JSON real spelling was rejected");
    auto emptyName = json;
    auto nameAt = emptyName.find("Dielectric");
    emptyName.replace(nameAt, std::strlen("Dielectric"), "");
    Check(ReadGLB(ImportGLB(emptyName, bin), -1).Materials[0].Values.Name == "Material 1",
          "Empty optional glTF name did not receive a native display fallback");
    auto rejects = [](auto f) {
        bool rejected = false;
        try
        {
            f();
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        Check(rejected, "Unsupported/malformed GLB was accepted");
    };
    for (const auto &change : std::vector<std::pair<std::string, std::string>>{
             {"\"translation\":[2,0,0]", "\"translation\":[2,0]"},
             {"\"metallicFactor\":0", "\"metallicFactor\":0,\"metallicFactor\":1"},
             {"\"name\":\"Unlit\"", "\"name\":\"Unlit\",\"alphaMode\":\"BLEND\""},
             {"\"POSITION\":0", "\"POSITION\":0,\"TEXCOORD_1\":1"},
             {"KHR_materials_unlit", "KHR_materials_transmission"}})
    {
        auto changed = json;
        auto where = changed.find(change.first);
        Check(where != std::string::npos, "Malformed test recipe missing");
        changed.replace(where, change.first.size(), change.second);
        rejects([&] { ReadGLB(ImportGLB(changed, bin), -1); });
    }
    auto corrupt = bin;
    corrupt[44] = char(255);
    rejects([&] { ReadGLB(ImportGLB(json, corrupt), -1); });
    rejects([&] { ReadGLB(bytes, -1, [] { return true; }); });
    auto primitive = data.Meshes[0].Primitives[0];
    auto repeated = primitive.Vertices;
    primitive.Vertices.insert(primitive.Vertices.end(), repeated.begin(), repeated.end());
    primitive.Vertices[4].UV.x = -1;
    primitive.Indices = {0, 1, 2, 3, 4, 5};
    ImportedMeshData mirrored{"mirrored", "Mirrored corners", {primitive}};
    auto split = CookMesh(mirrored, data.Materials, 71, {72});
    bool positive = false, negative = false;
    for (auto &vertex : split.Vertices)
    {
        positive |= vertex.Tangent.w == 1;
        negative |= vertex.Tangent.w == -1;
    }
    Check(split.Vertices.size() == 6 && positive && negative,
          "Mikk per-corner mirror split/reindex failed");
    ImportedImageData image{
        "exact", "Diagnostic", {2, 1, ImageFormat::RGBA8, {255, 0, 0, 63, 0, 255, 0, 192}}};
    auto tga = CookImage(image);
    auto pixels = Texture2D::ReadImageBytes(reinterpret_cast<const uint8_t *>(tga.data()),
                                            tga.size(), ImageFormat::RGBA8, 2);
    Check(pixels.Pixels == image.Pixels.Pixels,
          "Image cooking changed top-left rows/encoded color/data/alpha bytes");
    std::cout << "PASS: private GLB conversion, normalized accessors, shared instances/signed TRS, "
                 "strict malformed/unsupported/cancel rejection, format-independent Mikk "
                 "per-corner splitting and unchanged encoded image bytes\n";
}
