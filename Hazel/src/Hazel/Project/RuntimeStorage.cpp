#include "hzpch.h"
#include "RuntimeStorage.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Hazel {
bool RuntimeStorage::ValidName(const std::string& name) {
    auto stem=name.substr(0,name.find('.'));
    std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){return char(std::toupper(c));});
    if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" || (stem.size()==4 && (stem.substr(0,3)=="COM" || stem.substr(0,3)=="LPT") && stem[3]>='1' && stem[3]<='9'))return false;
    return !name.empty() && name.back()!='.'  && name.size()<=96 && name!="." && name!=".." &&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")==std::string::npos;
}
void RuntimeStorage::Configure(std::filesystem::path root, std::string owner, bool persistent) {
    if (!owner.empty() && !ValidName(owner)) throw std::invalid_argument("Invalid project save namespace");
    Clear(); m_Root=std::move(root); m_Owner=std::move(owner); m_Persistent=persistent;
}
void RuntimeStorage::Clear() { m_Overlay.clear(); m_Root.clear(); m_Owner.clear(); m_Persistent=false; }
std::filesystem::path RuntimeStorage::SlotPath(const std::string& slot) const {
    if (!ValidName(slot) || m_Owner.empty()) throw std::invalid_argument("Saving requires a project SaveNamespace and a portable slot name");
    return m_Root / m_Owner / ("slot-"+slot+".save");
}
std::string RuntimeStorage::Read(const std::string& slot) const {
    const auto path=SlotPath(slot);
    if(auto it=m_Overlay.find(slot);it!=m_Overlay.end()) return it->second;
    if(!m_Persistent || !std::filesystem::exists(path))return {};
    // Bound bytes before parsing; malformed/future files remain untouched.
    if(std::filesystem::file_size(path)>MaximumPayload*8+4096)throw std::runtime_error("Save file exceeds supported size");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read save file");
    std::string text((std::istreambuf_iterator<char>(input)),{});
    if(input.bad())throw std::runtime_error("Cannot read complete save file");
    auto node=YAML::Load(text);DocumentSchema::Structure(node);
    DocumentSchema::Keys(node,{"Version","Owner","Slot","Payload"},"Save");
    if(node["Version"].as<int>()!=1 || node["Owner"].as<std::string>()!=m_Owner || node["Slot"].as<std::string>()!=slot)
        throw std::runtime_error("Unsupported save version or ownership mismatch");
    auto payload=node["Payload"].as<std::string>();
    if(payload.empty() || payload.size()>MaximumPayload || payload.find('\0')!=std::string::npos)throw std::runtime_error("Invalid save payload");
    return payload;
}
void RuntimeStorage::Write(const std::string& slot, const std::string& payload) {
    const auto path=SlotPath(slot);
    if(payload.empty() || payload.size()>MaximumPayload || payload.find('\0')!=std::string::npos)throw std::invalid_argument("Save payload must be 1–65536 UTF-8 bytes without NUL");
    if(m_Persistent) {
        YAML::Emitter out;out<<YAML::BeginMap<<YAML::Key<<"Version"<<YAML::Value<<1
            <<YAML::Key<<"Owner"<<YAML::Value<<m_Owner<<YAML::Key<<"Slot"<<YAML::Value<<slot
            <<YAML::Key<<"Payload"<<YAML::Value<<payload<<YAML::EndMap;
        if(!out.good())throw std::runtime_error(out.GetLastError());
        std::filesystem::create_directories(path.parent_path());
        FileSystem::WriteFileAtomically(path,[&](auto& stream){stream<<out.c_str();});
    }
    m_Overlay[slot]=payload; // A failed disk write never publishes a false successful overlay.
}
}
