#pragma once
#include "Hazel/Assets/FontFamily.h"
#include "Hazel/Core/Resources.h"
#include <fstream>
// Small canonical fixture using the already shipped Apache-licensed face/notice.
inline void WriteTestFamily(const std::filesystem::path& root,const std::filesystem::path& family,const std::filesystem::path& face) {
    Hazel::FontFamilyDefinition def;def.Name="Fixture";def.Faces[0]={face,{"font-LICENSE.txt"}};
    auto notice=root/"font-LICENSE.txt";
    if(!std::filesystem::exists(notice))std::filesystem::copy_file(Hazel::Resources::Resolve("fonts/opensans/LICENSE.txt"),notice);
    std::filesystem::create_directories((root/family).parent_path());
    std::ofstream(root/family)<<Hazel::WriteFontFamilyText(def);
}
