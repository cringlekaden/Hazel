#pragma once
#include "Hazel/Scene/Entity.h"
namespace Hazel
{
// One reversible viewport gesture. Invalid samples retain the last valid edit;
// cancellation restores only our geometry, without overwriting unrelated edits.
class UIRectEditSession
{
  public:
    bool Begin(const Ref<Scene> &scene, uint64_t entity, unsigned edges);
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
    Ref<Scene> m_Scene;
    uint64_t m_Entity = 0, m_Identity = 0, m_Parent = 0;
    unsigned m_Edges = 0;
    float m_Scale = 1;
    GameUI::Rect m_ParentContent;
    UIRectComponent m_Initial, m_Last;
    std::string m_Error;
};
} // namespace Hazel
