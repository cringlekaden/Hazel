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
    glm::vec2 Available{}, Measured{};
    Rect Slot{};
    int GridRow=-1,GridColumn=-1,Line=-1;
    SizeMode WidthMode = SizeMode::Fixed, HeightMode = SizeMode::Fixed;
    bool MeasureResolved = false, Overflow = false;
    size_t MeasureIndex = std::numeric_limits<size_t>::max();
    float Scale = 1;
    size_t Order = 0, ButtonIndex = std::numeric_limits<size_t>::max();
    bool IsCanvas = false, Visible = false, Enabled = false, LayoutResolved = false;
    bool FocusEligible = false, ContainerOwned = false;
    HitPolicy Hit = HitPolicy::PassThrough;
};
struct ResolvedGuide {uint64_t Owner=0;int Axis=0,Index=0;bool Line=false;Rect Bounds{};};
struct ResolvedSnapshot
{
    uint64_t Generation = 0, SceneIdentity = 0, Modal = 0;
    SurfaceMetrics Surface;
    bool PreviewOrphans = false, Valid = false;
    std::vector<ResolvedElement> Elements;
    std::vector<size_t> PainterOrder;
    std::vector<ResolvedGuide> Guides;
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
// Last successful paint, separate from immutable arranged geometry. Read-only diagnostics.
struct UIImageVisual
{
    uint64_t Generation=0;
    glm::vec2 SourceSize{};
    ImageFit Fit=ImageFit::Stretch;
    ImagePlacement Placement;
};
struct RuntimeUIDiagnostics
{
    uint64_t SourceChecks = 0, Resolves = 0, Reuses = 0, InputGeneration = 0;
    uint64_t DroppedActions = 0;
    uint64_t Measurements = 0, MeasurementReuses = 0, Arrangements = 0;
    uint64_t ImageMeasurements = 0, ImageMeasurementReuses = 0;
    double LastResolveMilliseconds = 0, LastRoutingMilliseconds = 0;
    uint64_t Focus = 0, Capture = 0, Hover = 0, Modal = 0;
    bool InputEnabled = false, GameplayBlocked = false, PointerBlocked = false;
};
} // namespace Hazel::GameUI
