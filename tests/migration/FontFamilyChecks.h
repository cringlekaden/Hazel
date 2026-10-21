#pragma once
#include "FontFamilyFixture.h"
#include "Hazel/Assets/FontFamilyDocument.h"
static void FontFamilyChecks(const std::filesystem::path& temporary) {
    using namespace Hazel;
    const auto root=temporary/"families";std::filesystem::create_directories(root/"Faces");
    std::filesystem::copy_file(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),root/"Faces/Regular.ttf");
    WriteTestFamily(root,"UI.hfont","Faces/Regular.ttf");
    auto definition=ReadFontFamily(root/"UI.hfont");definition.Required="Hello café\n\t";definition.Sample="é Ω \u0301 مرحبا";
    Check(ReadFontFamilyText(WriteFontFamilyText(definition)).Required==definition.Required,"Family serialization lost coverage");
    auto text=WriteFontFamilyText(definition);
    Reject([&]{ReadFontFamilyText(text+"\nFontFamily: {}\n");},"Duplicate descriptor key accepted");
    auto duplicate=text;auto at=duplicate.find("Regular:");duplicate.insert(at,"Regular: {File: Faces/Regular.ttf, Notices: [font-LICENSE.txt]}\n    ");
    Reject([&]{ReadFontFamilyText(duplicate);},"Duplicate face slot accepted");
    Reject([&]{auto invalid=definition;invalid.Faces[0].File="../outside.ttf";invalid.Validate();},"Traversal face accepted");
    Reject([&]{auto invalid=definition;invalid.Faces[0].File="C:/Windows/font.ttf";invalid.Validate();},"Absolute face accepted");
    Reject([&]{auto invalid=definition;invalid.Faces[0].Notices.push_back("font-LICENSE.txt");invalid.Validate();},"Duplicate face notice accepted");
    Reject([&]{auto invalid=definition;invalid.Required=std::string(8193,'a');invalid.Validate();},"Unbounded corpus accepted");
    Reject([&]{auto invalid=definition;invalid.Required="\xed\xa0\x80";invalid.Validate();},"Malformed corpus accepted");
    auto generations=Hazel::Font::Resources().Generations;
    auto coverage=InspectFontFamily(root,definition);
    Check(Hazel::Font::Resources().Generations==generations,"Coverage inspection generated an atlas");
    Check(coverage.size()==1 && coverage[0].Required.Satisfied() && coverage[0].Sample.MissingAtlas>0 && coverage[0].Sample.Unsupported>0,"Source/atlas/typography diagnostics confused");
    auto greek=Hazel::Font::InspectCoverage(root/"Faces/Regular.ttf","Ω");
    Check(greek.MissingSource==0 && greek.MissingAtlas==1 && greek.Unsupported==0,"Source cmap incorrectly claimed atlas coverage or shaping requirement");
    Check(Hazel::Font::InspectCoverage(root/"Faces/Regular.ttf","\n\r\t\xef\xbb\xbf").Satisfied(),"Ordinary text controls reported missing");
    {
        auto assets=CreateRef<ProjectAssets>(root);FontFamilyDocument document(assets);document.Open("UI.hfont");
        auto regular=assets->FamilyFont({"UI.hfont"});
        auto status=assets->FamilyStatus({"UI.hfont",FontStyle::BoldItalic});
        Check(status.Actual==regular && status.StyleFallback && status.Selected==FontStyle::Regular && !status.DefaultFallback,"Absent style did not visibly select Regular");
        // A second family sharing a face must reuse live identity even without an LRU owner.
        WriteTestFamily(root,"Other.hfont","Faces/Regular.ttf");Hazel::Font::Trim();auto before=Hazel::Font::Resources().Generations;
        Check(assets->FamilyFont({"Other.hfont"})==regular && Hazel::Font::Resources().Generations==before,"Shared live face regenerated after eviction");
        document.Draft().Name="Edited";document.Changed();
        {std::ofstream out(root/"UI.hfont");out<<"broken external replacement";}
        Reject([&]{document.Save();},"Family save overwrote external edit");
        Check(document.Dirty() && document.Draft().Name=="Edited","Failed save discarded family draft");
        assets->Reload("UI.hfont");status=assets->FamilyStatus({"UI.hfont"});
        Check(status.Actual==regular && !status.Error.empty(),"Malformed descriptor replaced accepted family");
        std::ofstream(root/"UI.hfont")<<WriteFontFamilyText(definition);assets->Reload("UI.hfont");
        document.Open("UI.hfont");document.Draft().Name="Saved";document.Changed();document.Save();
        Check(!document.Dirty() && ReadFontFamily(root/"UI.hfont").Name=="Saved","Successful family save did not accept point");
        document.Draft().Name="Cancelled";document.Changed();document.Discard();Check(document.Draft().Name=="Saved" && !document.Dirty(),"Family discard leaked draft");
        auto files=AuditFontFamily(root,"UI.hfont");Check(files.size()==3,"Family/face/notice closure incomplete");
        document.Draft().Required="Ω";document.Changed();document.Save();
        Reject([&]{AuditFontFamily(root,"UI.hfont");},"Unmet required atlas coverage allowed export");
        document.Draft().Required.clear();document.Save();
        std::filesystem::remove(root/"font-LICENSE.txt");
        Reject([&]{AuditFontFamily(root,"UI.hfont");},"Missing notice allowed export");
        assets->Reload("UI.hfont");Check(assets->FamilyStatus({"UI.hfont"}).Actual==regular,"Missing dependency discarded a live accepted face");
        auto fallback=assets->FamilyStatus({"missing.hfont"});Check(fallback.DefaultFallback && !fallback.Error.empty(),"Missing family lacked fallback diagnostic");
        before=Hazel::Font::Resources().Generations;for(int i=0;i<50;++i)assets->FamilyFont({"missing.hfont"});
        Check(Hazel::Font::Resources().Generations==before,"Missing family retried font generation per lookup");
    }
    Hazel::Font::Trim();
}
