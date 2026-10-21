#include "Hazel/Assets/ProjectAssets.h"
#include <thread>
static void FontResourceChecks(const std::filesystem::path& root) {
    const auto regular=std::filesystem::absolute("assets/fonts/opensans/OpenSans-Regular.ttf");
    const auto bold=std::filesystem::absolute("assets/fonts/opensans/OpenSans-Bold.ttf");
    const auto a=root/"a.ttf",b=root/"b.ttf";
    auto copy=[](const auto& from,const auto& to){std::filesystem::copy_file(from,to,std::filesystem::copy_options::overwrite_existing);};
    copy(regular,a);copy(regular,b);
    const auto atlasBytes=Hazel::Font::GetDefault()->GetAtlasBytes();
    Hazel::Font::Shutdown();Hazel::FontLimits limits;limits.CacheEntries=1;limits.LiveAtlases=3;
    auto invalidLimits=limits;invalidLimits.CacheEntries=0;
    Reject([&]{Hazel::Font::Initialize(invalidLimits);},"Non-admitting cache configuration accepted");
    Hazel::Font::Initialize(limits);
    {
        auto fallback=Hazel::Font::GetDefault();
        std::filesystem::resize_file(a,128*1024);auto stamp=std::filesystem::last_write_time(a);
        auto first=Hazel::Font::Load(a);auto oldID=first->GetAtlasTexture()->GetRendererID();
        Check(Hazel::Font::Load(a)==first && Hazel::Font::Resources().Hits==1,"Content-key font reuse missing");
        copy(bold,a);std::filesystem::resize_file(a,128*1024);std::filesystem::last_write_time(a,stamp);
        auto second=Hazel::Font::Load(a,first);
        Check(second!=first && first->GetAtlasTexture()->GetRendererID()==oldID,"Font replacement mutated a live atlas");
        auto state=Hazel::Font::Resources();
        Check(state.CacheEntries==1 && state.LiveAtlases==3 && state.PinnedAtlases==3,"Cache versus pinned accounting wrong");
        auto heldAtlas=first->GetAtlasTexture();first.reset();
        Reject([&]{Hazel::Font::Load(b);},"Atlas-only pin did not prevent admission");
        Check(Hazel::Font::Resources().Generations==state.Generations,"Rejected admission generated an atlas");
        Hazel::ProjectAssets pressure(root);auto blocked=pressure.FontStatus("b.ttf");
        Check(blocked.Fallback && !blocked.Error.empty(),"Pinned admission did not expose fallback/diagnostic");
        for(int i=0;i<20;++i)pressure.FontAsset("b.ttf");
        Check(Hazel::Font::Resources().Generations==state.Generations,"Rejected project font regenerated every lookup");
        heldAtlas.reset();
        Check(pressure.FontStatus("b.ttf").Fallback,"Admission failure retried without explicit freshness boundary");
        pressure.Reload("b.ttf");auto third=pressure.FontAsset("b.ttf");
        Check(!pressure.FontStatus("b.ttf").Fallback,"Explicit retry did not recover from pin pressure");
        Check(Hazel::Font::Resources().CacheEntries==1 && Hazel::Font::Resources().LiveAtlases==3,"Cache/live limit violated");
        bool wrongThread=false;std::thread worker([&]{try{Hazel::Font::Load(a);}catch(const std::logic_error&){wrongThread=true;}});worker.join();
        Check(wrongThread,"Font graphics generation accepted the wrong thread");
    }
    Hazel::Font::Shutdown();Check(Hazel::Font::Resources().LiveAtlases==0,"Inactive atlases retained after shutdown");
    limits.LiveAtlases=16;limits.LiveAtlasBytes=2*atlasBytes;limits.CacheEntries=8;limits.CacheBytes=atlasBytes;limits.AtlasBytes=atlasBytes;Hazel::Font::Initialize(limits);
    {
        auto fallback=Hazel::Font::GetDefault();auto font=Hazel::Font::Load(a);
        Hazel::Renderer2D::BeginOrderedScene(Hazel::Camera(glm::mat4(1)),glm::mat4(1));
        Hazel::Renderer2D::DrawString("A",font,glm::mat4(1),{},91);
        font.reset();Hazel::Font::Trim();
        Check(Hazel::Font::Resources().LiveAtlases==2,"Pending draw failed to pin atlas");
        Reject([&]{Hazel::Font::Load(b);},"Pending draw pin ignored by admission");
        Hazel::Renderer2D::EndScene();
        Check(Hazel::Font::Resources().LiveAtlases==1,"Submitted draws retained atlas");
        Check(bool(Hazel::Font::Load(b)),"Admission did not recover after draw retirement");
    }
    Hazel::Font::Shutdown();
    Hazel::FontLimits rejected;rejected.AtlasDimension=32;Hazel::Font::Initialize(rejected);
    Reject([&]{Hazel::Font::Load(a);},"Small atlas dimension limit ignored");
    Check(Hazel::Font::Resources().LiveAtlases==0,"Dimension rejection published a partial atlas");
    Hazel::Font::Shutdown();rejected={};rejected.GeneratorBytes=64;Hazel::Font::Initialize(rejected);
    Reject([&]{Hazel::Font::Load(a);},"Generator scratch admission limit ignored");
    Check(Hazel::Font::Resources().Generations==0,"Scratch rejection generated an atlas");
    Hazel::Font::Shutdown();Hazel::Font::Initialize();
    {
        Hazel::ProjectAssets assets(root);copy(regular,a);
        auto accepted=assets.FontAsset("a.ttf");auto id=accepted->GetAtlasTexture()->GetRendererID();
        {std::ofstream out(a,std::ios::binary);out<<"broken replacement";}
        assets.Reload("a.ttf");auto failed=assets.FontStatus("a.ttf");
        Check(failed.Actual==accepted && !failed.Fallback && !failed.Error.empty(),"Failed reload lost accepted resource/reference");
        auto generations=Hazel::Font::Resources().Generations;
        for(int i=0;i<100;++i)Check(assets.FontAsset("a.ttf")==accepted,"Failed reload retried/changed resource");
        auto missing=assets.FontStatus("missing.ttf");Check(missing.Fallback && !missing.Error.empty(),"Missing font fallback/diagnostic absent");
        Check(Hazel::Font::Resources().Generations==generations,"Failure generated resources repeatedly");
        copy(bold,a);assets.Reload("a.ttf");auto recovered=assets.FontStatus("a.ttf");
        Check(recovered.Actual!=accepted && recovered.Error.empty() && !recovered.Fallback,"Font reload failed to recover");
        Check(accepted->GetAtlasTexture()->GetRendererID()==id,"Recovery invalidated live previous atlas");
        auto oversized=root/"oversized.ttf";
        {std::ofstream out(oversized,std::ios::binary);out.seekp(4*1024*1024);out.put('x');}
        Reject([&]{Hazel::Font::ValidateFile(oversized);},"Oversized font input accepted");
        auto before=Hazel::Font::Resources();
        Check(before.CacheEntries<=before.Limits.CacheEntries && before.CacheBytes<=before.Limits.CacheBytes && before.LiveAtlasBytes<=before.Limits.LiveAtlasBytes,"Resource budgets exceeded");
    }
    Check(Hazel::Font::Resources().CacheEntries==0,"Project retirement retained its cache entries");
    Hazel::Font::Shutdown();Check(Hazel::Font::Resources().LiveAtlases==0,"Project/default teardown leaked atlas");
    Hazel::Font::Initialize();
}
