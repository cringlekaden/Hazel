#pragma once
#include "Hazel/Renderer/RendererPolicy.h"
#include <filesystem>
#include <vector>
namespace Hazel {
    struct EditorRendererLaunch {
        RendererSettings Settings;
        bool VSync = true;
        std::filesystem::path Project;
        // CPU selection only. Reuses Stage D argument/session precedence and native codecs.
        static EditorRendererLaunch Read(const std::vector<std::string> &arguments);
    };
} // namespace Hazel
