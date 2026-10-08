#pragma once
#include <filesystem>
#include <string>
namespace Hazel
{
// CPU file ownership: accepted bytes, conflict checks, original recovery copy.
// A staged Open owns a candidate instance; failed Open never replaces the owner.
class FileDocument
{
public:
    void Open(const std::filesystem::path& path, bool preserveOriginal = false);
    void Check() const;
    void Save(const std::string& contents, const std::filesystem::path& recoveryRoot = {});
    const std::string& Original() const { return m_Bytes; }
    const std::filesystem::path& Path() const { return m_Path; }
    const std::filesystem::path& Backup() const { return m_Backup; }
    bool NeedsBackup() const { return m_PreserveOriginal; }
    void PreserveOriginal() { m_PreserveOriginal=true; }
    static std::string Read(const std::filesystem::path& path);
private:
    std::filesystem::path m_Path, m_Backup;
    std::string m_Bytes;
    bool m_PreserveOriginal = false;
};
}
