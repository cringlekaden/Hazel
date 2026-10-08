#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectCreation.h"
#include <iostream>
#include <map>
#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
int main(int argc, char **argv) {
    try {
        std::vector<std::string> args;
#ifdef HZ_PLATFORM_WINDOWS
        args = Hazel::WindowsCommandLineUTF8();
#else
        for (int i = 0; i < argc; ++i)
            args.emplace_back(argv[i]);
#endif
        if (args.size() == 2 && args[1] == "--contract") {
            std::cout << "HAZEL_AUTHORING_CONTRACT=1\n";
            return 0;
        }
        if (args.size() == 3 && args[1] == "validate") {
            Hazel::Log::Init();
            Hazel::DocumentLoadReport report;
            if (!Hazel::Project::LoadCandidate(std::filesystem::u8path(args[2]), &report))
                throw std::runtime_error(report.Error);
            std::cout << "Validated native project schema\n";
            return 0;
        }
        if (args.size() != 10 || args[1] != "create")
            throw std::runtime_error("Usage: HazelProject create --name NAME --identifier ID "
                                     "--destination FOLDER --templates FOLDER");
        std::map<std::string, std::string> values;
        for (size_t i = 2; i < args.size(); i += 2)
            if (!values.emplace(args[i], args[i + 1]).second)
                throw std::runtime_error("Duplicate generator argument");
        for (const auto *key : {"--name", "--identifier", "--destination", "--templates"})
            if (!values.count(key))
                throw std::runtime_error("Missing generator argument");
        Hazel::Log::Init();
        Hazel::Resources::Configure(Hazel::Resources::Defaults("HazelProject"));
        Hazel::ProjectCreateRequest request{values["--name"], values["--identifier"],
                                            std::filesystem::u8path(values["--destination"]),
                                            std::filesystem::u8path(values["--templates"])};
        const auto result = Hazel::ProjectCreation::Create(request);
        std::cout << "Created project: " << result.Descriptor.generic_u8string()
                  << "\nEditing ready; scripts intentionally not compiled\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Project creation: " << e.what() << '\n';
        return 1;
    }
}
