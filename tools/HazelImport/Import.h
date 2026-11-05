#pragma once
#include "ImportedSceneData.h"
#include "Hazel/Assets/ImportedAsset.h"
namespace Hazel::Import
{
struct Request
{
    std::filesystem::path Assets, Source,
        Destination; // absolute roots/source; asset-relative new folder
    int Scene = -1;
    Cancel Cancelled;
};
struct Result
{
    std::filesystem::path Manifest, Prefab;
    uint64_t Generation = 0;
    bool Unchanged = false;
};
Result ImportGLB(const Request &request);
Result ReimportGLB(const std::filesystem::path &assets, const std::filesystem::path &manifest,
                   const Cancel &cancel = {});
// Common publication entry point for adapters. Source bytes are checked again at
// acceptance; the DTO contains no parser-owned memory. Reimport reconciles by
// content evidence and refuses ambiguous mappings rather than guessing by index.
Result Publish(const Request &request, ImportedSceneData data, const std::string &sourceBytes,
               bool reimport);
} // namespace Hazel::Import
