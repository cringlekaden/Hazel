#pragma once
#include "UI/PropertyUI.h"
#include "UIEditCommand.h"
namespace Hazel
{
// Transient Inspector adapter. Both adapters write the document's shared history;
// this object stores no entries and never follows the new selection implicitly.
class UIInspectorEditing
{
  public:
    bool Edit(Scene &scene, DocumentHistory *history, uint64_t entity, const std::string &section,
              const UIEditValues &before, UIEditValues proposed,
              const PropertyUI::EditActivity &activity, uint32_t activeItem);
    void Poll(Scene &scene, DocumentHistory *history, uint64_t selected, bool editable,
              bool focused, bool escape, uint32_t activeItem);
    void Finish(Scene &scene, DocumentHistory *history, bool cancel);
    bool Active() const
    {
        return m_Entity != 0;
    }
    const std::string &Error() const
    {
        return m_Error;
    }

  private:
    uint64_t m_Document = 0, m_Entity = 0;
    uint32_t m_Item = 0;
    std::string m_Error;
};
} // namespace Hazel
