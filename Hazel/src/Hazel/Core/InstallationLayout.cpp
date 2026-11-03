#include "hzpch.h"
#include "InstallationLayout.h"
#include "FileSystem.h"

namespace Hazel {
InstallationLayout InstallationLayout::FromExecutable(const std::filesystem::path& executable) {
    const auto directory=executable.parent_path();
    if(directory.filename()=="MacOS" && directory.parent_path().filename()=="Contents" &&
       directory.parent_path().parent_path().extension()==".app") {
        const auto contents=directory.parent_path();
        const auto resources=contents/"Resources";
        return {directory,resources,contents/"Frameworks/Mono",resources/"Project",resources/"Project",resources/"build.json",true};
    }
    return {directory,directory/"Resources",directory/"mono",directory,directory/"Example",directory/"build.json"};
}
const InstallationLayout& InstallationLayout::Current() {
    static const auto layout=FromExecutable(FileSystem::GetExecutablePath());
    return layout;
}
}
