#pragma once
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/GPUTimer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/UI/RuntimeUISnapshot.h"
#include "Hazel/UI/MeasuredLayout.h"
#include <chrono>
#include <unordered_map>
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
    bool BlocksGameplay() const
    {
        return m_Resolved.Modal || m_KeyboardOwns || m_Input.Captured();
    }
    bool CapturesPointer() const
    {
        return m_Input.Captured() != 0;
    }
    bool BlocksPointer() const
    {
        return m_Resolved.Modal || m_PointerOver || m_Input.Captured();
    }
    bool Bounds(uint64_t entity, GameUI::Rect &pixels) const;
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
    struct Cache
    {
        GameUI::UIImageVisual PaintImage;
        std::vector<Renderer2D::TextGlyph> Glyphs;
        bool PreparedValid=false;
        glm::vec2 PreparedContent{-1};
        float PreparedSize=0,PreparedSpacing=0;
        GameUI::Align PreparedHorizontal=GameUI::Align::Start,PreparedVertical=GameUI::Align::Start;
        std::vector<Renderer2D::TextLine> Lines;
        std::string Text;
        FontSelection Selection;
        Ref<Font> FontAsset;
        std::array<Ref<const ResolvedSprite>, 5> Images;
        std::array<SpriteSource, 5> ImageSources;
        std::array<uint64_t, 5> ImageEpochs{};
        uint64_t Epoch = 0;
        float Width = -1, Kerning = 0;
        bool Wrap = false;
        std::array<std::vector<Renderer2D::TextLine>,2> MeasureLines;
        std::array<std::string,2> MeasureText;
        std::array<float,2> MeasureWidth{{-2,-2}}, MeasureKerning{};
        std::array<bool,2> MeasureWrap{};
        SpriteSource MeasureImageSource;
        uint64_t MeasureImageEpoch=0;
        glm::vec2 MeasureImageSize{-1,-1},MeasureResourceExtent{-1,-1};
    };
    void CaptureSources(GameUI::SurfaceMetrics surface, bool previewOrphans);
    void Build(GameUI::SurfaceMetrics surface, bool previewOrphans);
    void MeasureTree(Entity root, GameUI::Rect parent);
    glm::vec2 Intrinsic(Entity entity, float width);
    GameUI::MeasuredLayout m_Measure;
    std::vector<GameUI::MeasureNode> m_MeasureNodes;
    std::vector<uint64_t> m_MeasureIDs;
    void Visit(Entity entity, GameUI::Rect parent, GameUI::Rect clip, float scale, bool enabled,
               uint64_t canvas, size_t depth);
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
    bool m_PointerOver = false, m_KeyboardOwns = false, m_InputEnabled = false;
};
} // namespace Hazel
