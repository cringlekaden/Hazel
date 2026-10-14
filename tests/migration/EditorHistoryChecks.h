#pragma once
#include "Authoring/DocumentHistory.h"
#include "Hazel/Core/Resources.h"
#include "Authoring/UIEditCommand.h"
#include "Authoring/UIInspectorEditing.h"
namespace Hazel
{
struct HistoryFixture
{
    Ref<Scene> World=CreateRef<Scene>();Entity Canvas,Control;
    HistoryFixture() {
        World->SetAssets(Project::GetActive()->GetAssets());World->OnViewportResize(640,480);
        Canvas=World->CreateEntity("History canvas");Canvas.AddComponent<UICanvasComponent>().SafeInsets={};
        Control=World->CreateEntity("History control");World->Reparent(Control,Canvas,TransformPolicy::KeepLocal);
        Control.AddComponent<UIRectComponent>().SizeDelta={160,80};Control.AddComponent<TextComponent>().TextString="Original";
        Control.AddComponent<SpriteRendererComponent>();Control.AddComponent<UIButtonComponent>();
        World->RefreshUI();
    }
    std::string State() {return SceneSerializer(World).CaptureAuthoredState();}
    void Edit(DocumentHistory &history,float x) {
        auto v=UIEditValues::Capture(Control);
        Check(history.Begin(*World,CreateScope<UIEditCommand>(*World,Control.GetUUID(),"Position",v)),"History begin failed");
        v.Rect->Offset.x=x;std::string error;
        Check(dynamic_cast<UIEditCommand*>(history.Transaction())->Preview(*World,std::move(v),error),error.c_str());
        Check(history.Commit(*World),"History commit failed");
    }
};
static void EditorHistoryChecks()
{
    {
        HistoryFixture f,g;DocumentHistory history,prefab;
        history.Bind(f.World->GetIdentity());prefab.Bind(g.World->GetIdentity());
        const auto saved=f.State();history.MarkSaved(saved);prefab.MarkSaved(g.State());
        f.Edit(history,12);g.Edit(prefab,24);
        Check(history.Operations()==1&&prefab.Operations()==1&&history.Dirty(f.State()),"Independent document histories did not record edits");
        const auto untouched=g.State();
        Check(!history.Undo(*g.World)&&g.State()==untouched,"History applied to another document");
        Check(history.Undo(*f.World)&&f.State()==saved&&!history.Dirty(f.State())&&g.State()==untouched,"Undo/save point mixed documents");
        Check(history.Redo(*f.World)&&history.Dirty(f.State()),"Redo did not restore dirty state");
        Check(history.Undo(*f.World),"Branch setup Undo failed");f.Edit(history,33);
        Check(!history.CanRedo()&&history.Operations()==1,"New edit did not discard redo branch");
        history.MarkSaved(f.State());
        f.Control.GetComponent<UIRectComponent>().Padding.x=7;
        Check(history.Dirty(f.State()),"Cursor equality hid an out-of-history mutation");
        Check(history.Undo(*f.World)&&f.Control.GetComponent<UIRectComponent>().Padding.x==7&&history.Dirty(f.State()),"Undo overwrote unrelated native fields");
        Check(history.Redo(*f.World),"Redo after unrelated native mutation failed");
        auto current=f.State();f.Control.GetComponent<UIRectComponent>().Offset.x=99;current=f.State();
        Check(!history.Undo(*f.World)&&f.State()==current&&history.CanUndo(),"Conflicting native mutation was overwritten or advanced history");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());h.MarkSaved(f.State());
        UIInspectorEditing inspector;PropertyUI::EditActivity activity;activity.Item=100;activity.Label="Position";activity.Changed=true;
        const auto start=f.State();
        for(int i=1;i<=12;++i){auto before=UIEditValues::Capture(f.Control),next=before;next.Rect->Offset.x=float(i);
            Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"UI Rect",before,next,activity,100),"Continuous Inspector preview failed");}
        Check(h.Active()&&!h.Operations(),"Continuous Inspector made per-frame history entries");
        bool refused=false;try{h.Bind(999999);}catch(const std::logic_error&){refused=true;}
        Check(refused&&h.Active()&&h.Identity()==f.World->GetIdentity(),"History replacement silently abandoned an active edit");
        inspector.Poll(*f.World,&h,f.Control.GetUUID(),true,true,false,0);
        Check(!h.Active()&&h.Operations()==1&&h.UndoName().find("Position")!=std::string::npos,"Inspector deactivation did not commit once/name command");
        Check(h.Undo(*f.World)&&f.State()==start,"Inspector Undo did not restore authored values");
        auto before=UIEditValues::Capture(f.Control),next=before;next.Text->Text="Edited";activity.Item=101;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"Text",before,next,activity,101),"Text edit failed");
        const auto valid=f.State();before=UIEditValues::Capture(f.Control);next=before;next.Text->Text=std::string("bad\0text",8);
        Check(!inspector.Edit(*f.World,&h,f.Control.GetUUID(),"Text",before,next,activity,101)&&f.State()==valid,"Invalid Inspector text damaged last valid state");
        inspector.Poll(*f.World,&h,f.Control.GetUUID(),true,true,true,101);
        Check(f.State()==start&&!h.Active()&&h.CanRedo(),"Cancelled Inspector edit created history/discarded redo");
        before=UIEditValues::Capture(f.Control);next=before;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"No-op",before,next,activity,101)&&!h.Active(),"Unchanged Inspector edit created transaction");
        next.Rect->Offset.y=4;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"UI Rect",before,next,activity,101),"Selection cancellation setup failed");
        inspector.Poll(*f.World,&h,f.Canvas.GetUUID(),true,true,false,101);
        Check(f.State()==start&&!h.Active(),"Selection change committed to new entity or retained partial edit");
        before=UIEditValues::Capture(f.Control);next=before;next.Rect->Offset.y=8;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"UI Rect",before,next,activity,101),"Component cancellation setup failed");
        f.Control.RemoveComponent<TextComponent>();
        inspector.Poll(*f.World,&h,f.Control.GetUUID(),true,true,false,101);
        Check(!h.Active()&&!f.Control.HasComponent<TextComponent>()&&f.Control.GetComponent<UIRectComponent>().Offset.y==0,"Component removal retained unrelated partial edit or was undone");
        before=UIEditValues::Capture(f.Control);next=before;next.Rect->Offset.x=3;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"UI Rect",before,next,activity,101),"Native first-touch setup failed");
        f.Control.GetComponent<UIRectComponent>().Padding.x=14;
        before=UIEditValues::Capture(f.Control);next=before;next.Rect->Padding.x=18;
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"UI Rect",before,next,activity,101),"Native first-touch preview failed");
        inspector.Poll(*f.World,&h,f.Control.GetUUID(),true,true,false,0);
        Check(h.Undo(*f.World)&&f.Control.GetComponent<UIRectComponent>().Offset==glm::vec2(0)&&f.Control.GetComponent<UIRectComponent>().Padding.x==14,"Undo erased a native value introduced before that field joined the transaction");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());h.MarkSaved(f.State());
        auto &r=f.Control.GetComponent<UIRectComponent>();r.AnchorMax={1,1};r.SizeDelta={-100,-80};r.Pivot={.75f,.5f};r.MinimumSize={160,80};
        const auto start=f.State();f.World->RefreshUI();const auto bounds=f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds;
        UIRectEditSession gesture;Check(gesture.Begin(f.World,f.Control.GetUUID(),4,&h),"History resize begin failed");
        for(int i=1;i<=20;++i)Check(gesture.Update({float(i),0}),"History resize sample failed");
        const auto end=f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds;
        Check(end.Position.x==bounds.Position.x&&std::abs(end.Size.x-bounds.Size.x-20)<.001f&&r.SizeDelta.x<0&&!h.Operations(),"D6 math/history changed opposite edge or stretch delta");
        const auto valid=f.State();Check(!gesture.Update({NAN,0})&&f.State()==valid,"Invalid history resize changed last valid state");gesture.Finish(false);
        Check(h.Operations()==1&&h.Undo(*f.World)&&f.State()==start,"Complete resize did not create exactly one reversible entry");
        Check(h.Redo(*f.World),"Resize redo failed");const auto movedStart=f.State();
        Check(gesture.Begin(f.World,f.Control.GetUUID(),0,&h)&&gesture.Update({12,8}),"History move failed");gesture.Finish(true);
        Check(f.State()==movedStart&&h.Operations()==1,"Cancelled move leaked a partial command");
        Check(gesture.Begin(f.World,f.Control.GetUUID(),0,&h),"No-op gesture begin failed");gesture.Finish(false);Check(h.Operations()==1,"No-op gesture recorded history");
        const auto nativeOffset=r.Offset;Check(gesture.Begin(f.World,f.Control.GetUUID(),0,&h)&&gesture.Update({12,8}),"Native interruption setup failed");
        r.Visible=false;Check(!gesture.Update({13,9})&&!gesture.Active()&&r.Offset==nativeOffset&&!r.Visible,"Native gesture interruption overwrote native flag or retained move");
        r.Visible=true;auto group=f.World->CreateEntity("Flow");group.AddComponent<UIRectComponent>();group.AddComponent<UILayoutComponent>();f.World->Reparent(group,f.Canvas,TransformPolicy::KeepLocal);
        f.World->Reparent(f.Control,group,TransformPolicy::KeepLocal);f.Control.GetComponent<UIRectComponent>().AnchorMax={0,0};f.Control.GetComponent<UIRectComponent>().SizeDelta={160,80};
        Check(!gesture.Begin(f.World,f.Control.GetUUID(),0,&h)&&!h.Active(),"Flow position accepted an ineffective history gesture");
        f.World->Reparent(f.Control,f.Canvas,TransformPolicy::KeepLocal);
        const auto originalOffset=f.Control.GetComponent<UIRectComponent>().Offset;
        Check(gesture.Begin(f.World,f.Control.GetUUID(),0,&h)&&gesture.Update({8,0}),"Gesture retirement setup failed");
        f.Control.RemoveComponent<TextComponent>();
        Check(!gesture.Update({9,0})&&!gesture.Active()&&!h.Active()&&f.Control.GetComponent<UIRectComponent>().Offset==originalOffset&&!f.Control.HasComponent<TextComponent>(),"Retired component did not cancel gesture immediately/preserved replacement contract");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());f.Edit(h,8);
        auto value=f.Control.GetComponent<UIRectComponent>();f.Control.RemoveComponent<UIRectComponent>();f.Control.AddComponent<UIRectComponent>(value);
        const auto state=f.State();Check(!h.Undo(*f.World)&&f.State()==state,"Removed/re-added identical component accepted a stale command");
        h.Clear();f.Edit(h,9);auto id=f.Control.GetUUID();f.World->DestroyEntity(f.Control);
        f.Control=f.World->CreateEntityWithUUID(id,"Replacement");f.Control.AddComponent<UIRectComponent>().Offset.x=9;f.World->Reparent(f.Control,f.Canvas,TransformPolicy::KeepLocal);
        const auto replacement=f.State();Check(!h.Undo(*f.World)&&f.State()==replacement,"Recreated UUID accepted stale history");
        h.Clear();f.Edit(h,10);auto yaml=SceneSerializer(f.World).SerializeText();Check(SceneSerializer(f.World).DeserializeText(yaml),"Same-scene replacement fixture failed");
        const auto imported=f.State();Check(!h.Undo(*f.World)&&f.State()==imported,"Same-scene recovery/import reused entity lifetime tokens");
    }
    {
        HistoryFixture f;DocumentHistory h(2);h.Bind(f.World->GetIdentity());const auto saved=f.State();h.MarkSaved(saved);
        f.Edit(h,1);f.Edit(h,2);f.Edit(h,3);Check(h.Operations()==2&&!h.SavePointRetained()&&!h.Diagnostic().empty(),"Operation limit/save-point eviction lacked diagnostics");
        Check(h.Undo(*f.World)&&h.Undo(*f.World)&&!h.CanUndo()&&h.Dirty(f.State()),"Evicted history crossed the retained boundary");
        f.Control.GetComponent<UIRectComponent>().Offset.x=0;Check(f.State()==saved&&!h.Dirty(f.State()),"Exact saved content did not clear dirty state after save-point eviction");
        auto sample=CreateScope<UIEditCommand>(*f.World,f.Control.GetUUID(),"Position",UIEditValues::Capture(f.Control));
        const auto bytes=sample->Bytes();DocumentHistory memory(100,bytes*2+16);memory.Bind(f.World->GetIdentity());
        f.Edit(memory,4);f.Edit(memory,5);f.Edit(memory,6);Check(memory.UsedBytes()<=bytes*2+16&&memory.Operations()==2&&!memory.Diagnostic().empty(),"History memory eviction failed");
        DocumentHistory tiny(100,16);tiny.Bind(f.World->GetIdentity());const auto before=f.State();
        Check(!tiny.Begin(*f.World,std::move(sample))&&!tiny.Active()&&f.State()==before,"Oversized edit mutated or entered history");
        DocumentHistory growing(100,bytes+128);growing.Bind(f.World->GetIdentity());
        auto draft=UIEditValues::Capture(f.Control);std::string error;
        Check(growing.Begin(*f.World,CreateScope<UIEditCommand>(*f.World,f.Control.GetUUID(),"Position",draft)),"Bounded growing edit begin failed");
        draft.Text->Text=std::string(4096,'x');
        Check(!dynamic_cast<UIEditCommand*>(growing.Transaction())->Preview(*f.World,draft,error)&&f.State()==before,"Growing transaction exceeded its memory bound or published invalid state");
        Check(growing.Cancel(*f.World)&&!growing.Operations(),"Rejected growing edit left history");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());
        f.Control.AddComponent<UITextStyleComponent>();f.Control.AddComponent<UILayoutComponent>();f.Control.AddComponent<UINineSliceComponent>();
        const auto root=Project::GetAssetDirectory();std::filesystem::create_directories(root/"Fonts");
        std::filesystem::copy_file(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),root/"Fonts/history.ttf",std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(Resources::Resolve("fonts/opensans/LICENSE.txt"),root/"Fonts/history-LICENSE.txt",std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file("assets/textures/Checkerboard.png",root/"Textures/history-image.png",std::filesystem::copy_options::overwrite_existing);
        const auto start=f.State();std::string error;
        auto initial=UIEditValues::Capture(f.Control),proposed=initial;
        proposed.Rect->AnchorMin={.5f,0};proposed.Rect->AnchorMax={.5f,0};proposed.Rect->Pivot={.5f,0};proposed.Rect->MinimumSize={100,60};
        proposed.Flow->Gap=12;proposed.Flow->Cross=GameUI::Align::Center;
        proposed.Style->FontPath="Fonts/history.ttf";proposed.Style->Size=32;proposed.Style->Wrap=false;
        proposed.Slice->Borders={2,2,2,2};proposed.Slice->BorderScale=2;
        proposed.Button->Action="journal";proposed.Button->TabOrder=4;proposed.Button->Colors[1]={.8f,.9f,1,1};
        proposed.Text->Text="Longer journal caption";proposed.Text->Color={.7f,.8f,.9f,1};proposed.Text->Kerning=.01f;proposed.Text->LineSpacing=.2f;
        proposed.Image->Source=TextureSpriteSource{"Textures/history-image.png",0,{}};proposed.Image->Color={.9f,.8f,.7f,1};proposed.Button->Images[2]=proposed.Image->Source;
        Check(h.Begin(*f.World,CreateScope<UIEditCommand>(*f.World,f.Control.GetUUID(),"UI configuration",initial)),"Configuration transaction begin failed");
        Check(dynamic_cast<UIEditCommand*>(h.Transaction())->Preview(*f.World,proposed,error)&&h.Commit(*f.World),error.c_str());
        const auto changed=f.State();Check(h.Undo(*f.World)&&f.State()==start&&h.Redo(*f.World)&&f.State()==changed,"Typed flow/style/slice/button/text/image edit failed Undo/Redo");
        auto canvasBefore=UIEditValues::Capture(f.Canvas),canvasAfter=canvasBefore;canvasAfter.Canvas->ReferenceSize={1024,600};canvasAfter.Canvas->SafeInsets={8,8,8,8};canvasAfter.Canvas->SortOrder=3;canvasAfter.Canvas->Modal=true;
        Check(h.Begin(*f.World,CreateScope<UIEditCommand>(*f.World,f.Canvas.GetUUID(),"Canvas settings",canvasBefore)),"Canvas transaction begin failed");
        Check(dynamic_cast<UIEditCommand*>(h.Transaction())->Preview(*f.World,canvasAfter,error)&&h.Commit(*f.World)&&h.Undo(*f.World)&&f.State()==changed,"Canvas fields did not restore independently");
        // Captures never retain the procedural/native GPU resource bridge.
        auto texture=Texture2D::Create(TextureSpecification{});f.Control.GetComponent<SpriteRendererComponent>().SetTexture(texture);
        auto values=UIEditValues::Capture(f.Control);Check(!std::get<TextureSpriteSource>(values.Image->Source).Resource,"History capture retained a GPU resource");
        values.Image->Color={.2f,.3f,.4f,1};UIEditCommand color(*f.World,f.Control.GetUUID(),"Native color",UIEditValues::Capture(f.Control));
        Check(color.Preview(*f.World,values,error)&&std::get<TextureSpriteSource>(f.Control.GetComponent<SpriteRendererComponent>().Source).Resource==texture,"Color edit destroyed unrelated procedural image");color.Cancel(*f.World);
        values=UIEditValues::Capture(f.Control);values.Image->Source=std::monostate{};
        UIEditCommand generated(*f.World,f.Control.GetUUID(),"Generated source",UIEditValues::Capture(f.Control));
        Check(!generated.Preview(*f.World,values,error)&&std::get<TextureSpriteSource>(f.Control.GetComponent<SpriteRendererComponent>().Source).Resource==texture,"Source edit discarded a procedural image that history cannot restore");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());f.Control.AddComponent<UINineSliceComponent>();
        for(auto source:std::vector<SpriteSource>{TextureSpriteSource{},SpriteReference{}}){
            auto before=UIEditValues::Capture(f.Control),next=before;next.Image->Source=source;std::string error;
            Check(h.Begin(*f.World,CreateScope<UIEditCommand>(*f.World,f.Control.GetUUID(),"Image mode",before))&&dynamic_cast<UIEditCommand*>(h.Transaction())->Preview(*f.World,next,error)&&h.Commit(*f.World),"Unassigned image mode blocked the normal asset-picker workflow");
            Check(h.Undo(*f.World)&&UIEditValues::Capture(f.Control).Same(before)&&h.Redo(*f.World)&&UIEditValues::Capture(f.Control).Same(next),"Unassigned image mode was not reversible");
        }
        UIInspectorEditing inspector;auto before=UIEditValues::Capture(f.Control),next=before;next.Image->Source=std::monostate{};const auto count=h.Operations();
        Check(inspector.Edit(*f.World,&h,f.Control.GetUUID(),"Image assignment",before,next,{},799)&&!h.Active()&&h.Operations()==count+1,"Unwrapped asset/picker assignment was left pending across dialog focus events");
    }
    {
        HistoryFixture f;DocumentHistory h;h.Bind(f.World->GetIdentity());UIRectEditSession gesture;
        for(auto pivot:std::vector<glm::vec2>{{0,0},{.25f,.75f}})for(unsigned edge:{1u,2u,4u,8u}){
            auto &r=f.Control.GetComponent<UIRectComponent>();r.AnchorMin=r.AnchorMax={0,0};r.SizeDelta={80,40};r.MinimumSize={160,80};r.Pivot=pivot;r.Offset={};
            f.World->RefreshUI();const auto start=f.State();const auto bounds=f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds;
            Check(gesture.Begin(f.World,f.Control.GetUUID(),edge,&h)&&gesture.Update(edge==1?glm::vec2(-20,0):edge==2?glm::vec2(0,-20):edge==4?glm::vec2(20,0):glm::vec2(0,20)),"Constrained edge edit failed");gesture.Finish(false);
            auto b=f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds;
            Check(std::abs((edge==1?b.Position.x+b.Size.x:edge==2?b.Position.y+b.Size.y:edge==4?b.Position.x:b.Position.y)-(edge==1?bounds.Position.x+bounds.Size.x:edge==2?bounds.Position.y+bounds.Size.y:edge==4?bounds.Position.x:bounds.Position.y))<.001f,"Constrained pivoted edit moved opposite edge");
            Check(h.Undo(*f.World)&&f.State()==start,"Constrained edge history did not restore legacy authored values");
        }
    }
    std::cout<<"PASS: document history transactions/branches/save-point eviction/native changes/lifetime guards; one-entry Inspector and D6 viewport edits\n";
}
} // namespace Hazel
