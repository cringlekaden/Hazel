#pragma once
#include <filesystem>
namespace Hazel::NativeStarter
{
// Engine-authored, already cooked files: creation copies, never imports/bakes.
void Generate(const std::filesystem::path &assets);
void Ensure(const std::filesystem::path &templates, const std::filesystem::path &assets);
} // namespace Hazel::NativeStarter
