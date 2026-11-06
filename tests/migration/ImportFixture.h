#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <utility>
inline void ImportU32(std::string &bytes, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        bytes += char(v >> (8 * i));
}
inline std::string ImportGLB(const std::string &json, const std::string &bin)
{
    std::string padded = json;
    while (padded.size() % 4)
        padded += ' ';
    std::string binary = bin;
    while (binary.size() % 4)
        binary += '\0';
    std::string out = "glTF";
    ImportU32(out, 2);
    ImportU32(out, uint32_t(28 + padded.size() + binary.size()));
    ImportU32(out, uint32_t(padded.size()));
    ImportU32(out, 0x4e4f534a);
    out += padded;
    ImportU32(out, uint32_t(binary.size()));
    ImportU32(out, 0x004e4942);
    out += binary;
    return out;
}
inline std::pair<std::string, std::string> ImportFixture()
{
    std::string bin;
    for (float v : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f})
    {
        uint32_t n;
        std::memcpy(&n, &v, 4);
        ImportU32(bin, n);
    }
    for (uint8_t v : {0, 0, 255, 0, 0, 255, 0, 1, 2})
        bin += char(v);
    return {
        R"({"asset":{"version":"2.0","copyright":"Original diagnostic fixture (Apache-2.0)\nHazel maintainers"},"buffers":[{"byteLength":45}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":42,"byteLength":3}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5121,"normalized":true,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5121,"count":3,"type":"SCALAR"}],"materials":[{"name":"Dielectric","pbrMetallicRoughness":{"metallicFactor":0}},{"name":"Unlit","extensions":{"KHR_materials_unlit":{}}}],"meshes":[{"name":"Two slots","primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0},{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":1}]}],"nodes":[{"name":"Repeated A","mesh":0},{"name":"Repeated B","mesh":0,"translation":[2,0,0],"scale":[-1,1,1]}],"scenes":[{"nodes":[0,1]}],"scene":0,"extensionsUsed":["KHR_materials_unlit"]})",
        bin};
}
