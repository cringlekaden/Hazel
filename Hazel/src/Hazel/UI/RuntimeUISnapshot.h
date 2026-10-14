#pragma once
#include "RuntimeUIModel.h"
#include <limits>
#include <unordered_map>

namespace Hazel::GameUI
{
// Screen surface units are explicit. Future surface mappings can produce the
// same local geometry without exposing graphics or platform types here.
struct SurfaceMetrics
{
    glm::vec2 Viewport{}, Framebuffer{};
};
struct ResolvedElement
{
    uint64_t ID = 0, Parent = 0, Canvas = 0;
    Rect Bounds{}, ParentContent{}, Clip{}; // canvas logical units
    Rect PixelBounds{}, PixelClip{}, FramebufferBounds{}, FramebufferClip{};
    float Scale = 1;
    size_t Order = 0, ButtonIndex = std::numeric_limits<size_t>::max();
    bool IsCanvas = false, Visible = false, Enabled = false, LayoutResolved = false;
    bool FocusEligible = false, FlowOwned = false;
    HitPolicy Hit = HitPolicy::PassThrough;
};
struct ResolvedSnapshot
{
    uint64_t Generation = 0, SceneIdentity = 0, Modal = 0;
    SurfaceMetrics Surface;
    bool PreviewOrphans = false, Valid = false;
    std::vector<ResolvedElement> Elements;
    std::vector<size_t> PainterOrder;
    std::vector<Button> Buttons;
    std::unordered_map<uint64_t, size_t> Index;
    struct Warning
    {
        uint64_t Entity = 0;
        std::string Message;
    };
    std::vector<Warning> Warnings;
    const ResolvedElement *Find(uint64_t id) const
    {
        auto found = Index.find(id);
        return found == Index.end() ? nullptr : &Elements[found->second];
    }
};
struct RuntimeUIDiagnostics
{
    uint64_t SourceChecks = 0, Resolves = 0, Reuses = 0, InputGeneration = 0;
    uint64_t DroppedActions = 0;
    double LastResolveMilliseconds = 0, LastRoutingMilliseconds = 0;
    uint64_t Focus = 0, Capture = 0, Hover = 0, Modal = 0;
    bool InputEnabled = false, GameplayBlocked = false, PointerBlocked = false;
};
} // namespace Hazel::GameUI
