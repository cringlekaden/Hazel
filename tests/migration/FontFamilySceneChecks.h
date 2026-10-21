#pragma once
#include "FontFamilyFixture.h"
// Same family/style through Content measurement, glyph preparation, world drawing,
// scene copies and prefab metadata. Uses only shipped, licensed face bytes.
static void FontFamilySceneChecks(const std::filesystem::path& root) {
    auto directory=root/"family-scene";std::filesystem::create_directory(directory);
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",directory/"regular.ttf");
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Bold.ttf",directory/"bold.ttf");
    WriteTestFamily(directory,"Text.hfont","regular.ttf");auto def=ReadFontFamily(directory/"Text.hfont");
    def.Faces[1]={"bold.ttf",{"font-LICENSE.txt"}};std::ofstream(directory/"Text.hfont")<<WriteFontFamilyText(def);
    auto scene=CreateRef<Scene>();auto assets=CreateRef<ProjectAssets>(directory);scene->SetAssets(assets);scene->OnViewportResize(128,128);
    auto canvas=scene->CreateEntity("Family canvas");canvas.AddComponent<UICanvasComponent>().ReferenceSize={128,128};
    auto label=scene->CreateEntity("Family Content");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
    auto& rect=label.AddComponent<UIRectComponent>();rect.WidthMode=rect.HeightMode=SizeMode::Content;
    label.AddComponent<TextComponent>().TextString="iiiiiiii";label.GetComponent<TextComponent>().Font={"Text.hfont",FontStyle::Regular};
    label.AddComponent<UITextStyleComponent>().Size=16;
    RuntimeUI ui(*scene);ui.Resolve({128,128});ui.Draw();auto first=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x;auto generation=ui.Snapshot().Generation;
    auto regular=label.GetComponent<TextComponent>().ResolvedFont;
    label.GetComponent<TextComponent>().Font.Style=FontStyle::Bold; // direct native mutation between boundaries
    ui.Resolve({128,128});ui.Draw();auto bold=label.GetComponent<TextComponent>().ResolvedFont;
    auto width=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x;
    Check(bold!=regular && ui.Snapshot().Generation>generation && std::abs(first-width)>.1f && std::abs(width-Renderer2D::MeasureText("iiiiiiii",bold)*16)<.01f,"Style mutation split measurement/prepared face or missed Content invalidation");
    Check(ui.Diagnostics().PreparedRebuilds==1,"Face style change reused retired UVs");generation=ui.Snapshot().Generation;
    ui.Resolve({128,128});ui.Draw();Check(ui.Snapshot().Generation==generation && ui.Diagnostics().PreparedReuses==1,"Unchanged family bypassed caches");
    auto bytes=SceneSerializer(scene,directory).SerializeText();auto reopened=CreateRef<Scene>();SceneSerializer reader(reopened,directory,false,assets);
    Check(reader.DeserializeText(bytes) && reopened->GetEntityByUUID(label.GetUUID()).GetComponent<TextComponent>().Font==label.GetComponent<TextComponent>().Font,"Family/style scene roundtrip failed");
    auto copy=Scene::Copy(scene);auto copied=copy->GetEntityByUUID(label.GetUUID());
    Check(copied.GetComponent<TextComponent>().Font.Style==FontStyle::Bold && !copied.GetComponent<TextComponent>().ResolvedFont,"Scene copy retained derived font pin or lost authored style");
    auto duplicate=scene->DuplicateEntity(label);Check(duplicate.GetComponent<TextComponent>().Font==label.GetComponent<TextComponent>().Font,"Duplicate lost family/style");
    std::filesystem::create_directory(directory/"Prefabs");Prefab::Save(directory,"Prefabs/Label.hprefab",scene,label);
    auto instance=Prefab::Instantiate(directory,"Prefabs/Label.hprefab",*scene);
    Check(instance.GetComponent<TextComponent>().Font.Style==FontStyle::Bold && instance.GetComponent<TextComponent>().Font.Family=="Text.hfont","Prefab family/style roundtrip failed");
    // Failed changed-face path must retain the previous live accepted metrics/atlas.
    def.Faces[1].File="missing.ttf";std::ofstream(directory/"Text.hfont")<<WriteFontFamilyText(def);assets->Reload("Text.hfont");
    Check(assets->FamilyStatus({"Text.hfont",FontStyle::Bold}).Actual==bold,"Failed family replacement lost the active accepted face");
    ui.Resolve({128,128});ui.Draw();Check(label.GetComponent<TextComponent>().ResolvedFont==bold,"Failed family reload split measurement/draw resource");
    // World text uses the very same component resolution, independent of UI style.
    auto world=scene->CreateEntity("World family text");world.AddComponent<TextComponent>().TextString="World";
    world.GetComponent<TextComponent>().Font={"Text.hfont",FontStyle::Bold};
    Check(ResolveTextFont(world.GetComponent<TextComponent>(),assets)==bold,"World text resolved a different accepted face");
    const auto replacement=directory/"replacement";std::filesystem::create_directory(replacement);
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",replacement/"regular.ttf");
    WriteTestFamily(replacement,"Text.hfont","regular.ttf");auto secondAssets=CreateRef<ProjectAssets>(replacement);
    scene->SetAssets(secondAssets);ui.Resolve({128,128});ui.Draw();
    Check(label.GetComponent<TextComponent>().ResolvedFont!=bold && label.GetComponent<TextComponent>().ResolvedFont->GetPath().parent_path()==replacement,
          "Project asset-owner replacement reused old metrics/atlas at an equal epoch");
    scene->OnRuntimeStop();Check(!world.GetComponent<TextComponent>().ResolvedFont,"Stop retained runtime text resource state");
}
