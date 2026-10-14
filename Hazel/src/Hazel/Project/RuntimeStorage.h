#pragma once
#include <filesystem>
#include <map>
#include <string>

namespace Hazel {
// One runtime session owns its overlay. Editor Play never touches player data.
class RuntimeStorage {
public:
    static constexpr size_t MaximumPayload = 65536;
    static bool ValidName(const std::string& name);
    void Configure(std::filesystem::path root, std::string owner, bool persistent);
    void Clear();
    std::string Read(const std::string& slot) const; // Empty means no save. Invalid/corrupt data throws.
    void Write(const std::string& slot, const std::string& payload);
    bool IsPersistent() const { return m_Persistent; }
private:
    std::filesystem::path SlotPath(const std::string& slot) const;
    std::filesystem::path m_Root;
    std::string m_Owner;
    bool m_Persistent = false;
    std::map<std::string, std::string> m_Overlay;
};
}
