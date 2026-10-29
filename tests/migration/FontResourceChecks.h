#include "FontFamilyFixture.h"
#include "Hazel/Assets/ProjectAssets.h"
#include <thread>
static void FontResourceChecks(const std::filesystem::path& root) {
    using namespace Hazel;
    const auto regular=std::filesystem::absolute("assets/fonts/opensans/OpenSans-Regular.ttf");
    const auto bold=std::filesystem::absolute("assets/fonts/opensans/OpenSans-Bold.ttf");
    const auto a=root/"a.ttf",b=root/"b.ttf";
    auto copy=[](const auto& from,const auto& to){std::filesystem::copy_file(from,to,std::filesystem::copy_options::overwrite_existing);};
    auto page=[](const Ref<Font>& font,const char* text="A"){auto layout=font->Layout(text,-1,false);auto pages=font->PreparePages(*layout);Check(!pages.empty(),"Page preparation failed");return pages.front();};
    copy(regular,a);copy(regular,b);
    Font::Shutdown();FontLimits limits;limits.CacheEntries=1;limits.LiveAtlases=3;
    auto invalid=limits;invalid.CacheEntries=0;Reject([&]{Font::Initialize(invalid);},"Non-admitting cache accepted");Font::Initialize(limits);
    {
        auto fallback=Font::GetDefault();auto defaultPage=page(fallback);
        Reject([&]{fallback->Layout("A",-.5f,false);},"Fractional negative text width accepted as indefinite");
        Reject([&]{fallback->Layout("A",1,false,0,0,Text::Overflow::Ellipsis,0,-.5f);},"Fractional negative text height accepted as indefinite");
        std::filesystem::resize_file(a,128*1024);auto stamp=std::filesystem::last_write_time(a);
        auto first=Font::Load(a);auto firstPage=page(first);auto oldID=firstPage->Texture->GetRendererID();
        Check(Font::Load(a)==first,"Content-key reuse missing");
        copy(bold,a);std::filesystem::resize_file(a,128*1024);std::filesystem::last_write_time(a,stamp);
        auto second=Font::Load(a,first);auto secondPage=page(second);
        Check(first->GetCPUFace()!=second->GetCPUFace() && firstPage->Texture->GetRendererID()==oldID,"Replacement mutated accepted bytes/page");
        Check(Font::Resources().LiveAtlases==3 && Font::Resources().PinnedAtlases==3,"Cache/pin accounting wrong");
        auto heldAtlas=firstPage->Texture;firstPage.reset();first.reset();
        auto third=Font::Load(b);auto layout=third->Layout("A",-1,false);auto state=Font::Resources();
        Check(third->PreparePages(*layout).empty() && !third->PageError().empty(),"Texture-only pin ignored during page admission");
        for(int i=0;i<20;++i)third->PreparePages(*layout);
        Check(Font::Resources().Generations==state.Generations && Font::Resources().PageFailures==state.PageFailures+1,"Rejected page regenerated each lookup");
        heldAtlas.reset();Check(third->PreparePages(*layout).empty(),"Page failure retried without freshness");
        Font::Load(b,third);auto thirdPage=page(third);Check(third->PageError().empty(),"Explicit page retry failed");
        WriteTestFamily(root,"shared-pressure.hfont","a.ttf");ProjectAssets pressure(root);
        Check(pressure.FamilyFont({"shared-pressure.hfont"})==second,"Shared face duplicated after eviction");
        bool wrong=false;std::thread worker([&]{try{Font::Load(a);}catch(const std::logic_error&){wrong=true;}});worker.join();Check(wrong,"Font operation accepted wrong thread");
    }
    Font::Shutdown();Check(Font::Resources().LiveAtlases==0,"Shutdown retained pages");
    limits={};limits.LiveAtlases=1;Font::Initialize(limits);
    {
        auto font=Font::Load(a);auto glyphs=std::vector<Renderer2D::TextGlyph>{};Renderer2D::AppendTextGlyphs("A",font,{},glyphs);
        Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),glm::vec4(1),91);
        glyphs.clear();font.reset();Font::Trim();Check(Font::Resources().LiveAtlases==1,"Pending draw lost texture pin");
        auto other=Font::Load(b);auto layout=other->Layout("B",-1,false);Check(other->PreparePages(*layout).empty(),"Pending draw pin ignored");
        Renderer2D::EndScene();Check(Font::Resources().LiveAtlases==0,"Submitted draws retained texture");
        Font::Trim();Check(!other->PreparePages(*layout).empty(),"Admission did not recover after draw retirement");
    }
    Font::Shutdown();limits={};limits.AtlasDimension=32;Font::Initialize(limits);
    {auto font=Font::Load(a);auto layout=font->Layout("MW",-1,false);Check(font->PreparePages(*layout).empty() && Font::Resources().Generations==0,"Dimension rejection published a page");}
    Font::Shutdown();limits={};limits.GeneratorBytes=64;Font::Initialize(limits);
    {auto font=Font::Load(a);auto layout=font->Layout("M",-1,false);Check(font->PreparePages(*layout).empty() && Font::Resources().Generations==0,"Scratch rejection published page");}
    Font::Shutdown();Font::Initialize();
    {
        ProjectAssets assets(root);copy(regular,a);WriteTestFamily(root,"reload.hfont","a.ttf");auto accepted=assets.FamilyFont({"reload.hfont"});auto oldPage=page(accepted);auto id=oldPage->Texture->GetRendererID();
        {std::ofstream out(a,std::ios::binary);out<<"broken replacement";}
        assets.Reload("a.ttf");auto failed=assets.FamilyStatus({"reload.hfont"});
        Check(failed.Actual==accepted && !failed.DefaultFallback && !failed.Error.empty(),"Failed reload lost accepted face");
        // Previously unseen glyphs rasterize OLD accepted bytes after file corruption.
        Check(!accepted->PreparePages(*accepted->Layout(u8"Ω",-1,false)).empty(),"Old layout reopened corrupted pathname");
        auto generations=Font::Resources().Generations;for(int i=0;i<100;++i)Check(assets.FamilyFont({"reload.hfont"})==accepted,"Failed reload retried");
        Check(assets.FamilyStatus({"missing.hfont"}).DefaultFallback && Font::Resources().Generations==generations,"Missing font retry storm");
        copy(bold,a);assets.Reload("a.ttf");auto recovered=assets.FamilyStatus({"reload.hfont"});
        Check(recovered.Actual!=accepted && recovered.Error.empty() && oldPage->Texture->GetRendererID()==id,"Reload invalidated pinned page");
        WriteTestFamily(root,"missing.hfont","a.ttf");assets.Reload("missing.hfont");
        Check(assets.FamilyStatus({"missing.hfont"}).Actual==recovered.Actual &&
              assets.FamilyStatus({"missing.hfont"}).Error.empty(),"Missing-first-load family did not recover on explicit retry");
        assets.Refresh();Check(assets.FamilyFont({"reload.hfont"})==recovered.Actual,"Family refresh lost accepted identity");
        auto oversized=root/"oversized.ttf";{std::ofstream out(oversized,std::ios::binary);out.seekp(4*1024*1024);out.put('x');}
        Reject([&]{Font::ValidateFile(oversized);},"Oversized input accepted");
    }
    Check(Font::Resources().CacheEntries==0,"Project retained its reusable font cache");Font::Shutdown();Font::Initialize();
    {
        auto original=Font::Load(b);auto result=original->Layout("A",-1,false);auto pinned=page(original);auto cpu=original->GetCPUFace();
        original.reset();Font::Trim();auto before=Text::Service::Main().Usage();auto generations=Font::Resources().Generations;
        auto restored=Font::Load(b);
        Check(restored->GetCPUFace()==cpu && Text::Service::Main().Usage().Faces==before.Faces,
              "Page/result-only pin duplicated accepted CPU source after Font LRU eviction");
        Check(restored->PreparePages(*result).front()==pinned && Font::Resources().Generations==generations,
              "Page-only owner failed immutable page reuse");
    }
    Font::Shutdown();Font::Initialize();
    {
        auto fallback=Font::GetDefault();const auto padded=root/"cpu-pressure.ttf";copy(regular,padded);std::filesystem::resize_file(padded,4*1024*1024);
        std::vector<Ref<const Text::Face>> sources;
        for(int i=0;i<7;++i){{std::fstream f(padded,std::ios::binary|std::ios::in|std::ios::out);f.seekp(-1,std::ios::end);f.put(char(i+1));}sources.push_back(Font::Load(padded)->GetCPUFace());}
        auto before=Font::Resources();{std::fstream f(padded,std::ios::binary|std::ios::in|std::ios::out);f.seekp(-1,std::ios::end);f.put(8);}
        Reject([&]{Font::Load(padded);},"Pinned CPU inputs exceeded budget");
        Check(Font::Resources().Generations==before.Generations && Font::Resources().Evictions>before.Evictions,"CPU pressure skipped reusable eviction");
        sources[0].reset();Check(bool(Font::Load(padded)),"CPU admission failed after source pin released");
    }
    Font::Shutdown();Check(Text::Service::Main().Usage().Faces==0,"CPU source retention leaked");Font::Initialize();
}
