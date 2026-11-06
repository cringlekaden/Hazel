#pragma once
#include "Hazel/Assets/ImportedAsset.h"
inline void AssetCookingChecks()
{
    using namespace Hazel;
    Check(AssetHash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA256 empty vector");
    Check(AssetHash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA256 abc vector");
    Check(AssetHash(std::string(1000000, 'a')) ==
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "SHA256 multiblock vector");
    GeometryData data;
    data.Mesh = 17;
    data.Vertices.resize(3);
    data.Vertices[1].Position = {1, 0, 0};
    data.Vertices[2].Position = {0, 1, 0};
    data.Indices = {0, 1, 2};
    data.Primitives.push_back({42, 0, 3, 0, {}});
    data.UpdateBounds();
    auto bytes = EncodeGeometry(data);
    auto decoded = DecodeGeometry(bytes);
    Check(EncodeGeometry(decoded) == bytes && decoded.Primitives[0].Slot == 42,
          "Portable geometry byte-stable decoding failed");
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
        Check(rejected, "Malformed native asset accepted");
    };
    AssetNotice("Original attribution\nwith lines\tand tabs");
    rejects([&] { AssetNotice(std::string(65537, 'x')); });
    rejects([&] { AssetNotice(std::string("bad\0notice", 10)); });
    for (size_t size = 0; size < bytes.size(); ++size)
        rejects([&] { DecodeGeometry(std::string_view(bytes).substr(0, size)); });
    auto corrupt = bytes;
    corrupt.back() = 1;
    rejects([&] { DecodeGeometry(corrupt); });
    corrupt = bytes;
    corrupt[16] = 0;
    rejects([&] { DecodeGeometry(corrupt); });
    data.Indices[2] = 1;
    rejects([&] { EncodeGeometry(data); });
    data.Indices[2] = 2;
    data.Vertices[0].Normal.x = 1;
    rejects([&] { EncodeGeometry(data); });
    MaterialDefinition material;
    material.Name = "Canonical café";
    material.BaseColor = {.21404114f, .5f, 1, .3f};
    material.Textures[0].Image = "Colors.tga";
    material.Textures[2].Image = "Colors.tga";
    Check(ReadMaterialText(WriteMaterialText(material)) == material,
          "Typed material exact round trip failed");
    Check(MaterialTextureEncoding(MaterialTexture::BaseColor) == TextureEncoding::SRGB &&
              MaterialTextureEncoding(MaterialTexture::Normal) == TextureEncoding::Linear,
          "Shared image interpretation was aliased");
    auto colorSpec = MaterialTextureSpecification(MaterialTexture::BaseColor, material.Textures[0]);
    auto dataSpec = MaterialTextureSpecification(MaterialTexture::Normal, material.Textures[2]);
    Check(colorSpec.Format == ImageFormat::RGBA8 && dataSpec.Format == ImageFormat::RGBA8 &&
              colorSpec.Encoding == TextureEncoding::SRGB &&
              dataSpec.Encoding == TextureEncoding::Linear,
          "Material file interpretation does not specify canonical color/data realizations");
    auto future = YAML::Load(WriteMaterialText(material));
    future["Material"]["Version"] = 2;
    YAML::Emitter out;
    out << future;
    rejects([&] { ReadMaterialText(out.c_str()); });
    MeshDefinition mesh{
        17, "Triangle", "Triangle/Import.himport", {{42, "Surface", "Triangle/Surface.hmat"}}};
    Check(WriteMeshText(ReadMeshText(WriteMeshText(mesh))) == WriteMeshText(mesh),
          "Mesh document identity/slot round trip failed");
    std::cout << "PASS: NIST SHA256 vectors; portable geometry/checksum/count/range/degenerate "
                 "validation; strict typed material/mesh round trips and usage-derived color/data "
                 "interpretation\n";
}
