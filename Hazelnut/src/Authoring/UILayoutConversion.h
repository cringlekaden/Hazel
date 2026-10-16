#pragma once
#include "UIEditCommand.h"
namespace Hazel
{
// Transient preview. The reusable document history owns only the extracted
// authored command, never this preview scene or its resolved geometry.
class UILayoutConversion
{
  public:
    void Preview(const Ref<Scene> &scene, uint64_t root, bool freezeAmbiguous);
    bool Commit(Scene &scene, DocumentHistory &history);
    bool Valid() const { return m_Valid; }
    size_t Count() const { return m_Entries.size(); }
    const std::vector<std::string> &Notes() const { return m_Notes; }
    const std::string &Error() const { return m_Error; }
    struct Entry
    {
        uint64_t ID = 0;
        UIRectComponent Before, After;
        Scope<UIEditCommand> Guard;
    };

  private:
    uint64_t m_Document = 0, m_Generation = 0;
    bool m_Valid = false;
    std::vector<Entry> m_Entries;
    std::vector<std::string> m_Notes;
    std::string m_Error;
};
} // namespace Hazel
