#pragma once
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/GPUTimer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/UI/RuntimeUISnapshot.h"
#include "Hazel/UI/MeasuredLayout.h"
#include <chrono>
#include <unordered_map>
#include <optional>
namespace Hazel
{
// Scene-owned geometry and interaction. Authored components remain ordinary ECS
// values, including native raw writes. Resolve compares exact dependency bytes
// at input/render/delivery boundaries, without hashing or document serialization.
class RuntimeUI
{
  public:
    explicit RuntimeUI(Scene &scene) : m_Scene(scene)
    {
    }
    void Resolve(glm::vec2 viewport, bool previewOrphans = false)
    {
        ResolveSurface(GameUI::SurfaceMetrics{viewport, viewport}, previewOrphans);
    }
    void ResolveSurface(GameUI::SurfaceMetrics surface, bool previewOrphans = false);
    void UpdateInput(const GameUI::InputFrame &frame);
    void Draw(); // uses the published snapshot; host resolves before rendering
    bool Consume(uint64_t entity, GameUI::Action &action);
    bool ConsumeCancel(uint64_t scope);
    bool RequestScreen(GameUI::ScreenOperation operation,uint64_t screen=0);
    bool CommitScreens(); // called only at Scene's update boundaries
    bool IsScreenOpen(uint64_t screen) const;
    void SetPreviewScreen(uint64_t screen);
    bool HasScreenRequests() const {return !m_ScreenRequests.empty();}
    bool ConsumeEvent(uint64_t owner,GameUI::Action &event);
    bool Focus(uint64_t target);
    void ValidatePages(uint64_t target=0) const;
    void PreviewTab(uint64_t group,uint64_t tab); // zero removes Edit override
    void PreviewTooltip(uint64_t source,bool open);
    void Advance(float seconds); // one host update, never one input edge
    bool RequestTab(uint64_t group,uint64_t tab,bool notify);
    bool RequestDropdown(uint64_t id,bool open);
    bool DropdownOpen(uint64_t id) const;
    void PreviewDropdown(uint64_t id,bool open);
    void ValidateDropdowns(uint64_t target=0) const;
    bool OwnedPopup(uint64_t id) const;
    bool Checked(uint64_t id);
    void SetChecked(uint64_t id,bool value);
    uint64_t Selection(uint64_t group);
    void Select(uint64_t group,uint64_t member);
    float SliderValue(uint64_t id);
    void SetSliderValue(uint64_t id,float value);
    bool SliderEditing() const {return m_Slider.ID!=0;}
    GameUI::ScrollGeometry ScrollInfo(uint64_t id) const;
    void ScrollTo(uint64_t id,glm::vec2 offset,bool normalized=false);
    bool EnsureVisible(uint64_t view,uint64_t target,bool focus=false);
    void ValidateScrollViews(uint64_t target=0) const;
    bool ScrollVisual(uint64_t id,GameUI::Rect &bounds,bool &visible) const;
    float Progress(uint64_t id);
    void SetProgress(uint64_t id,float value);
    void ValidateControls(uint64_t target=0) const;
    bool ControlVisual(uint64_t id,GameUI::Rect &bounds,bool &visible) const;
    static constexpr size_t MaxScreens=64, MaxScreenRequests=64;
    bool BlocksGameplay() const
    {
        return m_ScrollNeutral || m_TransitionNeutral || m_Resolved.Modal || m_ActionOwnsFrame || m_KeyboardOwns || m_Input.Captured();
    }
    bool CapturesPointer() const
    {
        return m_Input.Captured() != 0;
    }
    bool BlocksPointer() const
    {
        return m_ScrollNeutral || m_TransitionNeutral || m_Resolved.Modal || m_ActionOwnsFrame || m_PointerOver || m_Input.Captured();
    }
    bool Bounds(uint64_t entity, GameUI::Rect &pixels) const;
    Ref<const Hazel::Text::Layout> TextLayout(uint64_t entity) const;
    bool ImageVisual(uint64_t entity,GameUI::UIImageVisual &result) const;
    const GameUI::ResolvedSnapshot &Snapshot() const
    {
        return m_Resolved;
    }
    GameUI::RuntimeUIDiagnostics Diagnostics() const;
    void Retire(uint64_t id);
    void CancelInput();
    void Reset();

  private:
    struct ScreenRequest;
    std::unordered_map<uint64_t,uint64_t> m_PageTabs,m_TooltipBodies;
    std::vector<uint64_t> m_TabGroups,m_TooltipIDs;
    std::unordered_map<uint64_t,uint64_t> m_TabPreviews;
    uint64_t m_PageFocus=0,m_PageFocusPage=0,m_PageFallback=0;
    bool m_RetainPageEvents=false;
    uint64_t m_TipCandidate=0,m_TipVisible=0,m_TipPreview=0;
    float m_TipElapsed=0;
    uint64_t m_TipLifetime=0,m_TipComponent=0;
    glm::vec2 m_TipMouse{};
    bool m_TipPointer=true;
    void CaptureCompositions();
    uint64_t PageSelection(uint64_t group) const;
    bool PageActive(uint64_t entity) const;
    void PreparePages();
    void FocusPage();
    bool CommitTab(const ScreenRequest &request);
    void ArrangeTooltip(bool measure);
    void AppendOverlay(Entity root,GameUI::Rect bounds,GameUI::Rect surface,float scale,uint64_t canvas,uint64_t scope,bool passive=false);
    void CompositionProblem(const std::string &message);
    enum class RequestKind {Screen,DropdownOpen,DropdownClose,DropdownChoice,TabChoice};
    std::vector<uint64_t> m_DropdownIDs;
    std::unordered_map<uint64_t,uint64_t> m_DropdownTriggers,m_DropdownPopups;
    uint64_t m_DropdownRevision=0,m_PreviewDropdown=0,m_DropdownFocus=0;
    bool m_ScopeCancellation=false;
    struct PendingChoice {GameUI::Action Event;uint64_t ReturnScope=0,OptionLifetime=0,OptionComponent=0;};
    std::optional<PendingChoice> m_Choice;
    size_t ReservedDropdownSlots() const;
    uint64_t ActiveDropdown() const;
    uint64_t DropdownGroup(uint64_t trigger) const;
    bool RequestChoice(uint64_t group,uint64_t option);
    void SyncDropdownScreens();
    bool CommitDropdownRequest(const ScreenRequest &request,bool &changed);
    void CloseOwnedPopup();
    void FinishDropdownChoice();
    void FocusDropdownOption();
    bool RouteDropdown(GameUI::InputFrame &input);
    void ArrangeDropdown(bool measure);
    const std::string &DisplayText(Entity entity) const;
    bool ProgrammaticScrollAvailable(uint64_t id) const;
    void DropdownProblem(const std::string &message);
    struct ScrollState {uint64_t Lifetime=0,Component=0,Content=0,ContentLifetime=0,ContentComponent=0;glm::vec2 Offset{};};
    struct BarCapture {uint64_t Owner=0,Track=0,Thumb=0,Component=0,Lifetime=0;int Axis=0;float Grab=0,MinimumThumb=0;bool Drag=false;GameUI::Rect Bounds;glm::vec2 Extent{},Viewport{},Surface{};} m_Bar;
    std::unordered_map<uint64_t,ScrollState> m_ScrollState;
    std::unordered_map<uint64_t,std::pair<uint64_t,int>> m_ScrollParts;
    std::vector<uint64_t> m_ScrollIDs;
    std::vector<GameUI::Button> m_NavigationButtons;
    std::vector<std::pair<uint64_t,glm::vec2>> m_RevealBackup;
    bool m_ScrollNeutral=false;
    uint64_t m_ScrollFocus=0;
    void PrepareScrollState();
    void ApplyScrollPresentation();
    void BuildHitTargets();
    void RepublishScroll();
    void ScrollProblem(const std::string &message);
    const GameUI::ScrollGeometry *ScrollNode(uint64_t id) const;
    bool ScrollAvailable(uint64_t id) const;
    bool ScrollEligible(uint64_t id) const;
    bool Descendant(uint64_t child,uint64_t ancestor) const;
    uint64_t HoverScroll(glm::vec2 framebufferPoint) const;
    bool RouteScroll(const GameUI::InputFrame &input);
    bool NavigateScroll(GameUI::InputFrame &input);
    void RouteScrollBar(const GameUI::Action &action);
    void CancelScrollCapture(bool neutral);
    void ReconcileScrollCapture();
    struct ScreenEntry { uint64_t ID=0,Lifetime=0,Component=0; GameUI::ScreenPolicy Policy;
        uint64_t OwnerLifetime=0,OwnerComponent=0;std::string OwnerClass;
        uint64_t Dropdown=0,DropdownLifetime=0,Trigger=0,TriggerLifetime=0,TriggerComponent=0,Canvas=0,ParentScope=0;};
    struct ScreenRequest {RequestKind Kind=RequestKind::Screen;GameUI::ScreenOperation Operation=GameUI::ScreenOperation::Open;uint64_t ID=0,Lifetime=0,Component=0;
        uint64_t Option=0,OptionLifetime=0,OptionComponent=0;
        uint64_t RootComponent=0,PresentationComponent=0,Page=0,PageLifetime=0,PageComponent=0;
        bool Notify=false;std::string Name;
        uint64_t Issuer=0,IssuerLifetime=0,IssuerComponent=0;std::string IssuerClass;};
    struct ScopeFocus {uint64_t Scope=0,Target=0;bool Keyboard=false;};
    std::vector<ScreenEntry> m_Screens;
    std::vector<uint64_t> m_OpenScreens;
    std::deque<ScreenRequest> m_ScreenRequests;
    std::vector<ScopeFocus> m_ScreenFocus;
    uint64_t m_PreviewScreen=0;
    void RememberFocus(uint64_t scope,uint64_t target,bool keyboard);
    uint64_t m_ScreenRevision=0,m_EventSequence=0,m_LastScope=0,m_ReturnFallback=0,m_PendingFocus=0;
    bool m_TransitionNeutral=false;
    std::chrono::steady_clock::time_point m_LastScreenWarning{};
    const ScreenEntry *Screen(uint64_t id) const;
    void SyncScreens();
    bool StampRequestIssuer(ScreenRequest &request);
    void AcceptScreenOwner(ScreenEntry &entry,const GameUI::ScreenPolicy &policy);
    void CancelScreenInput();
    void PublishScope();
    bool QueueEvent(GameUI::Action event);
    bool StampEvent(GameUI::Action &event);
    bool StampEventForScope(GameUI::Action &event,uint64_t scope);
    size_t ReservedSliderSlots() const;
    struct SliderEdit {
        uint64_t ID=0,Revision=0,Track=0,Fill=0,Thumb=0;
        float Start=0,Grab=0;
        bool Pointer=false;
        float Scale=1;
        glm::vec2 Viewport{},Framebuffer{};
        GameUI::SliderGeometry Geometry;
        GameUI::Action Terminal;
    } m_Slider;
    uint64_t m_NextGesture=0;
    bool m_SliderNeutral=false;
    GameUI::SliderGeometry SliderGeometryFor(uint64_t id) const;
    void SyncSliders();
    void ReconcileSlider();
    bool BeginSlider(uint64_t id,bool pointer,glm::vec2 point={});
    bool ChangeSlider(float value);
    void EndSlider(bool cancel,bool interrupted=false,bool deliver=true);
    void RouteSlider(const GameUI::Action &action);
    void AdjustSlider(int delta);
    struct ValueState {
        uint64_t Lifetime=0, Component=0, InitialSelection=0, Selected=0, SelectedLifetime=0,SelectedComponent=0;
        GameUI::ControlKind Kind=GameUI::ControlKind::Button;
        bool InitialChecked=false, Checked=false, Required=false, Tabs=false;
        uint64_t TabsComponent=0;
        float Minimum=0,Maximum=1,Initial=0,Value=0;
        GameUI::SteppedRange Range;
        uint64_t Revision=0;
        uint64_t Track=0,Fill=0,Thumb=0;
        GameUI::FillDirection Direction=GameUI::FillDirection::Right;
        uint64_t Owner=0,OwnerLifetime=0,OwnerComponent=0;
        std::string OwnerClass;
    };
    std::unordered_map<uint64_t,ValueState> m_Values;
    std::unordered_map<uint64_t,uint64_t> m_ControlVisuals;
    void SyncControls();
    void ControlProblem(const std::string &message);
    void ActivateControl(GameUI::Action event);
    uint64_t CanvasOf(uint64_t id) const;
    bool Member(uint64_t group,uint64_t id,bool eligible) const;
    bool Decoration(uint64_t owner,uint64_t visual) const;
    uint64_t ValueOwner(uint64_t source) const;
    bool EventValid(const GameUI::Action &event) const;
    void ScreenProblem(const std::string &message);
    std::vector<Renderer2D::TextGlyph> m_ScratchGlyphs;
    struct Cache
    {
        GameUI::UIImageVisual PaintImage;
        std::vector<Renderer2D::TextGlyph> Glyphs;
        bool PreparedValid=false;
        glm::vec2 PreparedContent{-1};
        float PreparedSize=0,PreparedSpacing=0;
        GameUI::Align PreparedHorizontal=GameUI::Align::Start,PreparedVertical=GameUI::Align::Start;
        std::vector<Renderer2D::TextLine> Lines;
        Ref<const Hazel::Text::Layout> PaintLayout;
        uint64_t PaintGeneration=0;
        std::string Text;
        FontSelection Selection;
        Ref<Font> FontAsset;
        std::array<Ref<const ResolvedSprite>, 5> Images;
        std::array<SpriteSource, 5> ImageSources;
        std::array<uint64_t, 5> ImageEpochs{};
        uint64_t Epoch = 0;
        float Width = -1, Kerning = 0, LineSpacing=0;
        bool Wrap = false;
        Hazel::Text::Overflow Overflow=Hazel::Text::Overflow::Clip;
        uint32_t MaxLines=0;
        float Height=-1;
        std::array<std::vector<Renderer2D::TextLine>,2> MeasureLines;
        std::array<std::string,2> MeasureText;
        std::array<float,2> MeasureWidth{{-2,-2}}, MeasureKerning{}, MeasureSpacing{};
        std::array<bool,2> MeasureWrap{};
        std::array<Hazel::Text::Overflow,2> MeasureOverflow{};
        std::array<uint32_t,2> MeasureMaxLines{};
        SpriteSource MeasureImageSource;
        uint64_t MeasureImageEpoch=0;
        glm::vec2 MeasureImageSize{-1,-1},MeasureResourceExtent{-1,-1};
    };
    void CaptureSources(GameUI::SurfaceMetrics surface, bool previewOrphans);
    void Build(GameUI::SurfaceMetrics surface, bool previewOrphans);
    void MeasureTree(Entity root, GameUI::Rect parent, const GameUI::Rect *ownedBox=nullptr, bool contentHeight=false);
    glm::vec2 Intrinsic(Entity entity, float width);
    GameUI::MeasuredLayout m_Measure;
    std::vector<GameUI::MeasureNode> m_MeasureNodes;
    std::vector<uint64_t> m_MeasureIDs;
    void Visit(Entity entity, GameUI::Rect parent, GameUI::Rect clip, float scale, bool enabled,
               uint64_t canvas, size_t depth, bool ownedRoot=false);
    bool Live(uint64_t id) const;
    void PruneActions();
    void WarnOverflow();
    Scene &m_Scene;
    GameUI::ResolvedSnapshot m_Resolved, m_Build;
    std::vector<uint8_t> m_Source, m_Candidate;
    std::vector<uint64_t> m_RectIDs, m_CanvasIDs;
    std::vector<Entity> m_Canvases;
    std::unordered_map<uint64_t, Cache> m_Cache;
    GameUI::InputRouter m_Input;
    std::deque<GameUI::Action> m_Actions;
    GameUI::RuntimeUIDiagnostics m_Diagnostics;
    Scope<GPUTimer> m_GPUTimer;
    uint64_t m_PaintSubmission=0;
    size_t m_PreparedBytes=0;
    std::weak_ptr<ProjectAssets> m_AssetOwner;
    uint64_t m_AssetGeneration=0;
    uint64_t m_NextGeneration = 0, m_OuterDrops = 0, m_ReportedDrops = 0;
    std::chrono::steady_clock::time_point m_LastWarning{};
    bool m_ActionOwnsFrame=false;
    bool m_PointerOver = false, m_KeyboardOwns = false, m_InputEnabled = false;
};
} // namespace Hazel
