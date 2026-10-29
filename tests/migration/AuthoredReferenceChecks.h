#pragma once
#include <functional>
#include "UICompositionFixture.h"
// Exercise ownership policy independently of control semantics (covered by the
// composed scene/session fixtures). No production visitor is used to seed or
// inspect expected references: each authored field has an explicit test case.
static void AuthoredReferenceChecks() {
    using namespace Hazel;
    struct Case {
        const char* Name;
        std::function<void(Entity, uint64_t)> Set;
        std::function<uint64_t(Entity)> Get;
    };
#define REF_CASE(Component, Member) {#Component "." #Member, \
    [](Entity e, uint64_t value) { \
        auto& c = e.HasComponent<Component>() ? e.GetComponent<Component>() : e.AddComponent<Component>(); \
        c.Member = value; }, \
    [](Entity e) { return uint64_t(e.GetComponent<Component>().Member); }}
    const Case cases[] = {
        REF_CASE(UICanvasComponent, Screen.InitialFocus),
        REF_CASE(UICanvasComponent, Screen.DefaultAction),
        REF_CASE(UICanvasComponent, Screen.CancelTarget),
        REF_CASE(UICanvasComponent, Screen.ReturnFocus),
        REF_CASE(UICanvasComponent, Screen.EventOwner),
        REF_CASE(UIButtonComponent, Indicator),
        REF_CASE(UIButtonComponent, Group),
        REF_CASE(UIButtonComponent, EventOwner),
        REF_CASE(UIRadioGroupComponent, InitialSelection),
        REF_CASE(UIRadioGroupComponent, EventOwner),
        REF_CASE(UIProgressComponent, Fill),
        REF_CASE(UISliderComponent, Track),
        REF_CASE(UISliderComponent, Fill),
        REF_CASE(UISliderComponent, Thumb),
        REF_CASE(UIScrollViewComponent, Content),
        REF_CASE(UIScrollViewComponent, HorizontalTrack),
        REF_CASE(UIScrollViewComponent, HorizontalThumb),
        REF_CASE(UIScrollViewComponent, VerticalTrack),
        REF_CASE(UIScrollViewComponent, VerticalThumb),
        REF_CASE(UIDropdownComponent, Trigger),
        REF_CASE(UIDropdownComponent, Popup),
        REF_CASE(UITabComponent, Page),
        REF_CASE(UITabComponent, InitialFocus),
        REF_CASE(UITooltipComponent, Body),
        {"Script.Entity", [](Entity e, uint64_t value) {
            if (!e.HasComponent<ScriptComponent>()) e.AddComponent<ScriptComponent>();
            auto& field = ScriptEngine::GetScriptFieldMap(e)["Target"];
            field.Field = {ScriptFieldType::Entity, "Target", nullptr};field.SetValue(value);
            auto& number = ScriptEngine::GetScriptFieldMap(e)["Number"];
            number.Field = {ScriptFieldType::ULong, "Number", nullptr};number.SetValue<uint64_t>(102);
        }, [](Entity e) { return ScriptEngine::GetScriptFieldMap(e).at("Target").GetValue<uint64_t>(); }}
    };
#undef REF_CASE
    for (const auto& item : cases) {
        auto scene = CreateRef<Scene>();
        auto root = scene->CreateEntityWithUUID(101, item.Name);
        auto child = scene->CreateEntityWithUUID(102, "Internal target");
        scene->Reparent(child, root, TransformPolicy::KeepLocal);
        scene->CreateEntityWithUUID(999, "Allowed external duplicate target");
        item.Set(root, 102);
        auto copy = scene->DuplicateEntity(root);
        auto copiedChild = scene->GetChildren(copy.GetUUID()).front();
        Check(item.Get(copy) == uint64_t(copiedChild) && copiedChild != child.GetUUID(), item.Name);
        Check(item.Get(root) == 102, "Duplicate mutated source reference");
        if (root.HasComponent<ScriptComponent>())
            Check(ScriptEngine::GetScriptFieldMap(copy).at("Number").GetValue<uint64_t>() == 102,
                  "Visitor remapped a numeric script field as an Entity");
        auto detached = Scene::ExtractSubtree(root);
        auto detachedRoot = detached->GetEntityByUUID(101);
        Prefab::Validate({}, detached, false);
        item.Set(detachedRoot, 999);
        const auto accepted = SceneSerializer(detached).CaptureAuthoredState();
        bool rejected = false;
        try { Prefab::Validate({}, detached, false); } catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && SceneSerializer(detached).CaptureAuthoredState() == accepted,
              "Prefab ownership allowed an escape or mutated its authored data");
        // Zero remains an ownership-neutral reference. Required-part semantics
        // belong to controls, not the shared field enumeration.
        item.Set(detachedRoot, 0);Prefab::Validate({}, detached, false);
        item.Set(root, 999);
        auto externalCopy = scene->DuplicateEntity(root);
        Check(item.Get(externalCopy) == 999, "Allowed external duplicate reference was rewritten");
        const auto before = SceneSerializer(scene).CaptureAuthoredState();
        rejected = false;
        try { Scene::ExtractSubtree(root); } catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && SceneSerializer(scene).CaptureAuthoredState() == before,
              "Failed detached extraction partially changed source identities/references");
    }
    // The very same native auditor invoked once by Python packaging must reject
    // escaped tab focus / script Entity fields, and return prefab asset closure.
    const auto directory=std::filesystem::temp_directory_path()/
        ("hazel-reference-audit-"+std::to_string(uint64_t(UUID())));
    std::filesystem::create_directories(directory);
    struct Cleanup {std::filesystem::path Path;~Cleanup(){std::error_code e;std::filesystem::remove_all(Path,e);}} cleanup{directory};
    auto composition=CompositionFixture();
    auto tabs=Scene::ExtractSubtree(composition->GetEntityByUUID(200));
    auto tabsRoot=tabs->GetEntityByUUID(200);
    tabsRoot.AddComponent<ScriptComponent>().ClassName="Authored.ReferenceFixture";
    auto& target=ScriptEngine::GetScriptFieldMap(tabsRoot)["Target"];
    target.Field={ScriptFieldType::Entity,"Target",nullptr};target.SetValue<uint64_t>(210);
    Prefab::Save(directory,"tabs.hprefab",tabs,tabsRoot,true);
    const auto path=directory/"tabs.hprefab";
    const auto acceptedFile=FileDocument::Read(path);
    Check(AuditSpriteAssets(directory,{"tabs.hprefab"}).empty(),"Valid authored-reference prefab failed native audit");
    for (bool script : {false,true}) {
        auto doc=YAML::Load(acceptedFile);
        for(auto node:doc["Entities"]) {
            if(!script && node["Entity"].as<uint64_t>()==210)node["UITabComponent"]["InitialFocus"]=99999999;
            if(script && node["Entity"].as<uint64_t>()==200)node["ScriptComponent"]["ScriptFields"][0]["Data"]=99999999;
        }
        YAML::Emitter out;out<<doc;std::ofstream(path)<<out.c_str();
        const auto invalid=FileDocument::Read(path);bool rejected=false;
        try{AuditSpriteAssets(directory,{"tabs.hprefab"});}
        catch(const std::runtime_error& error){rejected=std::string(error.what()).find("External entity reference")!=std::string::npos;}
        Check(rejected && FileDocument::Read(path)==invalid,"Native package auditor allowed an escaped reference or rewrote input");
    }
    std::ofstream(path)<<acceptedFile;
    auto holder=CreateRef<Scene>();auto owner=holder->CreateEntity("Prefab dependency");
    owner.AddComponent<ScriptComponent>().ClassName="Authored.ReferenceFixture";
    auto& dependency=ScriptEngine::GetScriptFieldMap(owner)["Asset"];
    dependency.Field={ScriptFieldType::Prefab,"Asset",nullptr};dependency.AssetReference="tabs.hprefab";
    SceneSerializer(holder,directory).Serialize((directory/"holder.hazel").generic_u8string());
    auto closure=AuditSpriteAssets(directory,{"holder.hazel","tabs.hprefab"});
    Check(std::find(closure.begin(),closure.end(),"tabs.hprefab")!=closure.end(),"Native audit omitted a script prefab dependency from package inventory checks");
    std::filesystem::remove(path);bool rejected=false;
    try{AuditSpriteAssets(directory,{"holder.hazel"});}catch(const std::runtime_error&){rejected=true;}
    Check(rejected,"Native audit accepted a missing script prefab dependency");
    std::cout << "PASS: all 24 authored UI references and script Entity fields: internal remap, "
                 "external duplicate retention, optional zero, atomic prefab/extraction rejection and native package closure\n";
}
