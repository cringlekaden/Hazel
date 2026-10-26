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
    uint64_t ID = 0, Parent = 0, Canvas = 0, Scope = 0;
    bool OverlayRoot=false, PassiveOverlay=false;
    uint64_t Page=0;
    Rect LayoutBounds{}, LayoutParentContent{}, LayoutSlot{};
    glm::vec2 Displacement{};
    uint64_t ScrollParent=0;
    Rect Bounds{}, ParentContent{}, Clip{}; // canvas logical units
    Rect PixelBounds{}, PixelClip{}, FramebufferBounds{}, FramebufferClip{}, InheritedClip{};
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
struct ScrollGeometry {
    uint64_t ID=0,Content=0,Parent=0;
    Rect Viewport;
    glm::vec2 Extent{},Offset{},Range{};
    std::array<Rect,2> Tracks{},Thumbs{};
};
struct PageGeometry {uint64_t Group=0,Tab=0,Page=0;};
struct TooltipGeometry {uint64_t Source=0,Body=0;Rect Bounds;glm::vec2 Natural{};};
struct DropdownGeometry {uint64_t ID=0,Trigger=0,Popup=0,ParentScope=0,ParentModal=0,Selection=0; Rect Anchor,Bounds; bool Above=false;};
struct ResolvedGuide {uint64_t Owner=0;int Axis=0,Index=0;bool Line=false;Rect Bounds{},LayoutBounds{};};
struct ResolvedSnapshot
{
    uint64_t Generation = 0, SceneIdentity = 0, Modal = 0, Scope = 0;
    SurfaceMetrics Surface;
    bool PreviewOrphans = false, Valid = false;
    std::vector<ResolvedElement> Elements;
    std::vector<size_t> PainterOrder;
    std::vector<ResolvedGuide> Guides;
    std::vector<ScrollGeometry> Scrolls;
    std::vector<DropdownGeometry> Dropdowns;
    std::vector<PageGeometry> Pages;
    std::vector<TooltipGeometry> Tooltips;
    std::vector<Button> Buttons;
    std::vector<ScreenState> Screens;
    std::unordered_map<uint64_t, size_t> Index;
    enum class WarningCode { Interrupted, Orphan, NestedCanvas, ContainerRect, ButtonRect, Overflow, MeasureOverflow, ScreenTarget };
    struct Warning
    {
        uint64_t Entity = 0;
        WarningCode Code;
        const char *Message() const
        {
            switch(Code){
                case WarningCode::ScreenTarget:return "Screen target is missing or outside its canvas; choose a live target in Properties";
                case WarningCode::Interrupted:return "Interrupted UI rect chain: an ancestor has no UI Rect/Canvas";
                case WarningCode::Orphan:return "Orphan UI rect: runtime requires a Canvas ancestor";
                case WarningCode::NestedCanvas:return "Nested/parented Canvas resolves independently against the viewport";
                case WarningCode::ContainerRect:return "UI Flow needs a UI Rect";
                case WarningCode::ButtonRect:return "UI Button needs a UI Rect";
                case WarningCode::Overflow:return "Resolved rect overflows its parent content";
                case WarningCode::MeasureOverflow:return "Measured layout overflows available space; minimums are retained";
            }
            return "Unknown UI warning";
        }
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
    double LastSourceMilliseconds=0, LastLayoutMilliseconds=0;
    double LastPrepareMilliseconds=0, LastSubmitMilliseconds=0;
    uint32_t VisibleElements=0, SubmittedElements=0, DrawCalls=0, UploadGroups=0;
    uint32_t ClipTransitions=0,PrimitiveTransitions=0,FontTransitions=0;
    std::array<uint32_t,6> FlushReasons{}; // Renderer2D::FlushReason order
    uint64_t PaintSubmission=0,GPUSubmission=0,PreparedRebuilds=0,PreparedReuses=0,PreparedBytes=0;
    double LastGPUMilliseconds=0;
    bool GPUAvailable=false;
    uint64_t Scope=0, NavigationMisses=0;
    uint64_t ScrollPublications=0,ScrollRoutes=0,ScrollReveals=0,ScrollCapture=0;
    double LastScrollMilliseconds=0;
    std::string ScrollProblem;
    std::string CompositionProblem;
    uint64_t PageChanges=0, Tooltip=0;
    uint64_t TextEdit=0,TextCaret=0,TextAnchor=0,TextHistoryBytes=0,TextEventBytes=0;
    std::string TextError;
    std::string DropdownProblem;
    uint64_t Dropdown=0, DropdownChoices=0, DropdownRejected=0;
    uint64_t SliderEdit=0, SliderGesture=0;
    bool SliderPointer=false;
    uint64_t EventOwner=0, PendingEvents=0, RetiredEvents=0, PendingScreens=0, RejectedScreens=0;
    std::string ScreenProblem, ControlProblem;
    uint64_t RejectedChanges=0;
    uint64_t Focus = 0, Capture = 0, Hover = 0, Modal = 0;
    bool InputEnabled = false, GameplayBlocked = false, PointerBlocked = false;
};
} // namespace Hazel::GameUI
