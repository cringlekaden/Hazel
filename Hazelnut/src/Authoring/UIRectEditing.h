#pragma once
#include "Hazel/Scene/Entity.h"
#include "DocumentHistory.h"
namespace Hazel
{
// One reversible viewport gesture. Invalid samples retain the last valid edit;
// cancellation restores only our geometry, without overwriting unrelated edits.
class UIRectEditSession
{
  public:
    bool Begin(const Ref<Scene> &scene, uint64_t entity, unsigned edges, DocumentHistory *history = nullptr);
    bool Update(glm::vec2 logicalDelta);
    void Finish(bool cancel);
    bool Active() const
    {
        return bool(m_Scene);
    }
    uint64_t EntityID() const
    {
        return m_Entity;
    }
    const std::string &Error() const
    {
        return m_Error;
    }
    float Scale() const
    {
        return m_Scale;
    }

  private:
    void Remember(const GameUI::ResolvedElement &node);
    Ref<Scene> m_Scene;
    DocumentHistory *m_History = nullptr; // borrowed owner, never stored in a command
    uint64_t m_Entity = 0, m_Identity = 0, m_Parent = 0;
    unsigned m_Edges = 0;
    float m_Scale = 1;
    GameUI::Rect m_ExpectedSlot;
    int m_ExpectedRow=-1,m_ExpectedColumn=-1,m_ExpectedLine=-1;
    GameUI::Rect m_ParentContent, m_ExpectedParentContent, m_InitialBounds;
    bool m_ContainerOwned = false;
    UIRectComponent m_Initial, m_Last;
    std::string m_Error;
};
} // namespace Hazel
