#pragma once
#include "Hazel/Core/Base.h"
#include "Hazel/Renderer/Font.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace Hazel {
const char* FontStyleName(FontStyle style);
struct FontSelection {
    std::filesystem::path Family; // empty = engine Open Sans family
    FontStyle Style=FontStyle::Regular;
    void Validate() const;
    bool operator==(const FontSelection& other) const {return Family==other.Family && Style==other.Style;}
    bool operator!=(const FontSelection& other) const {return !(*this==other);}
};
struct FontFaceDefinition {
    std::filesystem::path File;
    std::vector<std::filesystem::path> Notices;
};
struct FontFamilyDefinition {
    std::string Name="New family";
    std::array<FontFaceDefinition,4> Faces;
    std::string Required, Sample="The quick brown fox 0123456789";
    void Validate() const;
    FontStyle Select(FontStyle requested) const; // absent style explicitly uses Regular; no synthesis
};
struct FamilyCoverage {
    FontStyle Style=FontStyle::Regular;
    FontCoverage Required, Sample;
};
FontFamilyDefinition ReadFontFamily(const std::filesystem::path& path);
FontFamilyDefinition ReadFontFamilyText(const std::string& text);
std::string WriteFontFamilyText(const FontFamilyDefinition& definition);
// CPU only, bounded input. Paths in descriptors are relative to the asset root, not the descriptor.
std::vector<FamilyCoverage> InspectFontFamily(const std::filesystem::path& root,const FontFamilyDefinition& definition);
std::vector<std::filesystem::path> AuditFontFamily(const std::filesystem::path& root,const std::filesystem::path& reference);
// Export/authoring validation: CPU only, actual requested face runs and source.
void AuditTextEmphasis(const std::filesystem::path& root,const FontSelection& selection,const std::string& source,const std::vector<Text::Span>& spans);
const FontFamilyDefinition& DefaultFontFamily();
std::filesystem::path DefaultFontFace(FontStyle requested);
}
