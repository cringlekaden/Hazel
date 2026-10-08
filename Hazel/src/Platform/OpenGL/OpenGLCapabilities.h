#pragma once
#include "Hazel/Renderer/RendererPolicy.h"

namespace Hazel::OpenGLCapabilities {
// Refreshed after GLAD initialization; standalone resource tests initialize
// lazily on their current, loaded context. Rendering remains single-threaded.
void Initialize();
void Reset();
const RendererCapabilities& Get();
const RendererSettings& GetSettings();
const RendererResolution& GetResolution();
void Configure(const RendererSettings& requested);
bool UseShaderBinaries();
}
