#include "FontFamilyFixture.h"
// Accepted resource revisions must invalidate both measurement and prepared glyphs.
static void FontReloadSceneChecks(const std::filesystem::path& root) {
    const auto directory=root/"font-reload";std::filesystem::create_directory(directory);
    const auto file=directory/"face.ttf";
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",file);
    auto scene=CreateRef<Scene>();auto assets=CreateRef<ProjectAssets>(directory);scene->SetAssets(assets);scene->OnViewportResize(128,128);
    auto canvas=scene->CreateEntity("Font reload canvas");auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={128,128};c.SafeInsets={};
    auto label=scene->CreateEntity("Content label");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
    auto& rect=label.AddComponent<UIRectComponent>();rect.WidthMode=rect.HeightMode=SizeMode::Content;
    label.AddComponent<TextComponent>().TextString="iiiiiiii";
    auto& style=label.AddComponent<UITextStyleComponent>();style.Size=16;WriteTestFamily(directory,"face.hfont","face.ttf");label.GetComponent<TextComponent>().Font.Family="face.hfont";
    RuntimeUI ui(*scene);ui.Resolve({128,128});ui.Draw();
    float before=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x;
    auto accepted=assets->FamilyFont({"face.hfont"});auto generation=ui.Snapshot().Generation;
    {std::ofstream out(file,std::ios::binary);out<<"invalid replacement";}
    assets->Reload("face.ttf");ui.Resolve({128,128});ui.Draw();
    Check(assets->FamilyFont({"face.hfont"})==accepted && ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x==before,"Failed replacement split measurement/drawing versions");
    Check(ui.Snapshot().Generation>generation && ui.Diagnostics().PreparedRebuilds==1,"Explicit failed freshness boundary skipped preparation invalidation");
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Bold.ttf",file,std::filesystem::copy_options::overwrite_existing);
    assets->Reload("face.ttf");ui.Resolve({128,128});ui.Draw();
    auto actual=assets->FamilyFont({"face.hfont"});auto after=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x;
    Check(actual!=accepted && std::abs(after-before)>.1f && std::abs(after-Renderer2D::MeasureText("iiiiiiii",actual)*16)<.01f,"Accepted font replacement did not remeasure Content");
    Check(ui.Diagnostics().PreparedRebuilds==1,"Accepted font reused retired prepared UVs");
    rect.WidthMode=SizeMode::Fixed;rect.FixedSize.x=30;label.GetComponent<TextComponent>().TextString="iiiiii iiiiii iiiiii";
    ui.Resolve({128,128});ui.Draw();auto wrapped=ui.Snapshot().Find(label.GetUUID())->Bounds.Size;
    std::filesystem::remove(file);assets->Reload("face.ttf");ui.Resolve({128,128});ui.Draw();
    Check(ui.Snapshot().Find(label.GetUUID())->Bounds.Size==wrapped && assets->FamilyFont({"face.hfont"})==actual,"Missing replacement broke wrapping/drawing consistency");
    label.GetComponent<TextComponent>().Font.Family="missing.hfont";ui.Resolve({128,128});ui.Draw();
    Check(assets->FamilyStatus({"missing.hfont"}).DefaultFallback && label.GetComponent<TextComponent>().Font.Family=="missing.hfont","Fallback rewrote authored font reference");
    scene->ValidateUI(true);
    SceneSerializer serializer(scene,directory);auto bytes=serializer.SerializeText();
    auto reopened=CreateRef<Scene>();SceneSerializer reader(reopened,directory,false,assets);
    Check(reader.DeserializeText(bytes) && reader.Report().State==DocumentLoadState::EditableWithProblems,"Missing-font document did not reopen with a repair diagnostic");
    Check(reopened->GetEntityByUUID(label.GetUUID()).GetComponent<TextComponent>().Font.Family=="missing.hfont","Reopen silently rewrote a missing font");
    std::ofstream(directory/"missing.hazel")<<bytes;
    bool rejected=false;try{AuditSpriteAssets(directory,{"missing.hazel"});}catch(const std::exception&){rejected=true;}
    Check(rejected,"Invalid export closure unexpectedly passed");
    generation=ui.Snapshot().Generation;ui.Resolve({128,128});ui.Draw();
    Check(ui.Snapshot().Generation==generation && ui.Diagnostics().PreparedReuses==1,"Fallback bypassed snapshot/prepared reuse");
}
