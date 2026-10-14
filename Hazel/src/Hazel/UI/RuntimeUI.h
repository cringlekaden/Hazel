#pragma once
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/UI/RuntimeUIModel.h"
#include <unordered_map>
namespace Hazel
{
// Per-scene transient state. Configuration stays in ordinary components.
class RuntimeUI
{
  public:
    explicit RuntimeUI(Scene &scene) : m_Scene(scene)
    {
    }
    void Resolve(glm::vec2 viewport, bool previewOrphans = false);
    void UpdateInput(const GameUI::InputFrame &frame);
    void Draw();
    bool Consume(uint64_t entity, GameUI::Action &action);
    bool BlocksGameplay() const
    {
        return m_Modal != 0 || m_KeyboardOwns || m_Input.Captured()!=0;
    }
    bool CapturesPointer() const {return m_Input.Captured()!=0;}
    bool BlocksPointer() const
    {
        return m_Modal != 0 || m_PointerOver || m_Input.Captured() != 0;
    }
    bool Bounds(uint64_t entity, GameUI::Rect &pixels) const;
    void Retire(uint64_t id);
    void Reset();

  private:
    struct Node
    {
        uint64_t ID = 0;
        Entity Element;
        GameUI::Rect Bounds, Clip;
        float Scale = 1;
        bool Enabled = true;
        uint64_t Canvas = 0;
    };
    struct Cache
    {
        std::vector<GameUI::FlowItem> Items;
        std::vector<GameUI::Rect> Arranged;
        std::vector<Renderer2D::TextLine> Lines;
        std::string Text;
        std::filesystem::path FontPath;
        Ref<Font> FontAsset;
        uint64_t Epoch = 0;
        float Width = -1, Kerning = 0;
        bool Wrap = false;
    };
    void Visit(Entity entity, GameUI::Rect parent, GameUI::Rect clip, float scale, bool enabled,
               uint64_t canvas, size_t depth, const GameUI::Rect *arranged = nullptr);
    bool Live(uint64_t id) const;
    Scene &m_Scene;
    glm::vec2 m_Viewport{};
    std::vector<Node> m_Nodes;
    std::vector<Entity> m_Canvases;
    std::vector<GameUI::Button> m_Buttons;
    std::unordered_map<uint64_t, Cache> m_Cache;
    GameUI::InputRouter m_Input;
    std::deque<GameUI::Action> m_Actions;
    uint64_t m_Modal = 0;
    bool m_PointerOver = false, m_KeyboardOwns = false;
};
} // namespace Hazel
