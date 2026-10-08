#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace Hazel {
    struct ProjectCreateRequest {
        std::string Name, Identifier;
        std::filesystem::path Destination, Templates;
        int TemplateVersion = 1;
    };
    struct ProjectCreateResult {
        std::filesystem::path Descriptor;
        std::vector<std::filesystem::path> Files;
    };
    // Native content generation only. No compiler/interpreter, active session or
    // GPU.
    class ProjectCreation {
      public:
        static constexpr int ContractVersion = 1;
        static ProjectCreateResult Create(const ProjectCreateRequest &request);
        static void ValidateTemplates(const std::filesystem::path &folder);
    };
} // namespace Hazel
