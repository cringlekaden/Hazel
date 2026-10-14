#include "UIInspectorEditing.h"
namespace Hazel
{
void UIInspectorEditing::Finish(Scene &scene, DocumentHistory *history, bool cancel)
{
    if (Active() && history && history->Identity() == m_Document)
    {
        if (cancel)
            history->Cancel(scene);
        else if (!history->Commit(scene))
        {
            history->Cancel(scene);
        }
        if (!history->Diagnostic().empty())
            m_Error = history->Diagnostic();
    }
    m_Document = m_Entity = 0;
    m_Item = 0;
}
void UIInspectorEditing::Poll(Scene &scene, DocumentHistory *history, uint64_t selected,
                              bool editable, bool focused, bool escape, uint32_t activeItem)
{
    if (!Active())
        return;
    if (!history || history->Identity() != m_Document || !history->Transaction() ||
        scene.GetIdentity() != m_Document || selected != m_Entity || !editable || !focused ||
        escape)
    {
        Finish(scene, history, true);
        return;
    }
    try
    {
        history->Transaction()->Check(scene);
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        Finish(scene, history, true);
        return;
    }
    if (!activeItem)
        Finish(scene, history, false);
}
bool UIInspectorEditing::Edit(Scene &scene, DocumentHistory *history, uint64_t entity,
                              const std::string &section, const UIEditValues &before,
                              UIEditValues proposed, const PropertyUI::EditActivity &activity,
                              uint32_t activeItem)
{
    const auto item = activeItem ? activeItem : activity.Item;
    if (Active() && activity.Deactivated && !activity.Changed &&
        (!activity.Item || activity.Item == m_Item))
        Finish(scene, history, false);
    if (before.Same(proposed))
        return true;
    try
    {
        if (Active() && (m_Document != scene.GetIdentity() || m_Entity != entity ||
                         (item && m_Item && item != m_Item) || !item))
            Finish(scene, history, false);
        if (!history)
        {
            UIEditCommand command(scene, entity, section, before);
            return command.Preview(scene, before, std::move(proposed), m_Error);
        }
        if (history->Identity() != scene.GetIdentity())
            throw std::runtime_error("Inspector belongs to a different document");
        if (!Active())
        {
            auto name = section + (activity.Label.empty() ? "" : ": " + activity.Label);
            if (!history->Begin(scene,
                                CreateScope<UIEditCommand>(scene, entity, std::move(name), before)))
                throw std::runtime_error(history->Diagnostic());
            m_Document = scene.GetIdentity();
            m_Entity = entity;
            m_Item = item;
        }
        auto command = dynamic_cast<UIEditCommand *>(history->Transaction());
        if (!command || command->EntityID() != entity)
            throw std::runtime_error("Another document transaction owns this edit");
        if (!command->Preview(scene, before, std::move(proposed), m_Error))
            return false;
        // Browse/drop/picker buttons may publish authored values without a
        // continuous property item. Accept those assignments before queued
        // platform dialog focus events can cancel an unfinished transaction.
        if (activity.Committed || !activity.Changed || !activeItem)
            Finish(scene, history, false);
        return true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        return false;
    }
}
} // namespace Hazel
