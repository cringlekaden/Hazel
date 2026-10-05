#include "hzpch.h"
#include "FileDocument.h"
#include "FileSystem.h"
#include "UUID.h"
#include <fstream>
#include <chrono>

namespace Hazel
{
std::string FileDocument::Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Missing/unreadable document: " + path.generic_u8string());
    const auto size=input.tellg();
    if (size < 0 || size > 64*1024*1024) throw std::runtime_error("Document exceeds the 64 MiB supported limit");
    std::string bytes(static_cast<size_t>(size), '\0');
    input.seekg(0);
    if (!bytes.empty() && !input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read complete document: " + path.generic_u8string());
    return bytes;
}
void FileDocument::Open(const std::filesystem::path& path, bool preserveOriginal)
{
    auto bytes=Read(path);
    m_Path=std::filesystem::absolute(path).lexically_normal();
    m_Bytes=std::move(bytes);m_Backup.clear();m_PreserveOriginal=preserveOriginal;
}
void FileDocument::Check() const
{
    bool unchanged=false;
    try { unchanged=!m_Path.empty() && Read(m_Path)==m_Bytes; } catch(const std::exception&) {}
    if (!unchanged)
        throw std::runtime_error("Document changed on disk: " + m_Path.generic_u8string() +
                                ". Draft and disk retained. Save Copy, guarded Reopen, or Cancel; no automatic merge.");
}
void FileDocument::Save(const std::string& contents, const std::filesystem::path& recoveryRoot)
{
    Check();
    if (m_PreserveOriginal && m_Backup.empty())
    {
        if (recoveryRoot.empty()) throw std::runtime_error("Recovery location required before replacing original data");
        std::filesystem::create_directories(recoveryRoot);
        // Bounded history: 20 originals / 128 MiB. Originals are never source files.
        std::vector<std::filesystem::directory_entry> copies;
        uintmax_t bytes=0;
        for (const auto& entry : std::filesystem::directory_iterator(recoveryRoot))
            if (entry.is_regular_file() && entry.path().extension()==".original")
            { copies.push_back(entry); bytes+=entry.file_size(); }
        std::sort(copies.begin(),copies.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
        auto count=copies.size();
        for (const auto& entry : copies)
        {
            if (count<20 && bytes+m_Bytes.size()<=128*1024*1024) break;
            bytes-=entry.file_size();std::filesystem::remove(entry.path());
            auto metadata=entry.path();metadata+=".path";std::filesystem::remove(metadata);
            --count;
        }
        auto backup=recoveryRoot/(std::to_string(static_cast<uint64_t>(UUID()))+".original");
        FileSystem::WriteFileAtomically(backup,[&](auto& out){out<<m_Bytes;},WriteMode::CreateNew);
        uint64_t fingerprint=14695981039346656037ull;
        for(unsigned char byte:m_Bytes){fingerprint^=byte;fingerprint*=1099511628211ull;}
        auto metadata=backup;metadata+=".path";
        FileSystem::WriteFileAtomically(metadata,[&](auto& out){out<<m_Path.generic_u8string()<<"\nBytes: "<<m_Bytes.size()<<"\nSaved at: "
            <<std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())<<"\nFNV-1a-64: "<<std::hex<<fingerprint;},WriteMode::CreateNew);
        m_Backup=std::move(backup);
    }
    Check(); // Revalidate after backup work, immediately before atomic publication.
    FileSystem::WriteFileAtomically(m_Path,[&](auto& out){out<<contents;});
    m_Bytes=contents;m_PreserveOriginal=false;
}
}
