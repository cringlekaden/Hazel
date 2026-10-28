#include "Hazel/Scripting/ManagedString.h"
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
// Real internal calls, isolated persistent storage, and deferred scene/instance boundaries.
static void ManagedStringChecks(const Ref<Project>& project, const std::filesystem::path& directory) {
    const mono_unichar2 diagnostic[] = {'a', 0, 0xd83d, 0xde80, 'b'};
    auto* message = mono_string_new_utf16(mono_domain_get(), diagnostic, 5);
    Check(ManagedStrings::Read(message, 5, 7, "Diagnostic", ManagedStrings::NulPolicy::Preserve) ==
          std::string("a\0", 2) + u8"🚀b", "Diagnostic conversion truncated NUL/supplementary text");
    Check(ManagedStrings::Read(nullptr, 0, 0, "Optional").empty(), "Null-to-empty convention changed");
    const auto root = project->GetAssetRoot();
    const auto prefabPath = std::filesystem::u8path(u8"Prefabs/café 🚀.hprefab");
    const auto scenePath = std::filesystem::u8path(u8"Scenes/café 🚀.hazel");
    std::filesystem::create_directories(root / "Prefabs");
    auto prefab = CreateRef<Scene>();
    auto child = prefab->CreateEntity("String child");
    Prefab::Save(root, prefabPath, prefab, child);
    SceneSerializer(Make("String target"), root).Serialize((root / scenePath).generic_u8string());
    auto authored = Make("String probe", "Migration.ManagedStringProbe");
    authored->CreateEntity(u8"café 🚀");
    RuntimeSession session;
    session.Resize(800, 400);
    session.Start(project, authored);
    // No environment or player-profile changes: configure this session's storage
    // before the fixture's first OnUpdate, in its disposable directory only.
    const auto saves = directory / "string-save-root";
    session.Storage().Configure(saves, "managed-strings", true);
    auto scene = session.GetScene();
    const auto count = scene->GetAllEntitiesWith<IDComponent>().size();
    auto instance = ScriptEngine::GetEntityScriptInstance(2);
    session.Update(.016f);
    Check(instance->GetFieldValue<bool>("Passed"), "Managed string/save/path rejection probe failed");
    session.Update(.016f);
    Check(session.GetScene() == scene && ScriptEngine::GetEntityScriptInstance(2) == instance &&
          scene->GetAllEntitiesWith<IDComponent>().size() == count,
          "Malformed path queued a truncated-prefix transition or instantiated content");
    RuntimeStorage disk;
    disk.Configure(saves, "managed-strings", true);
    Check(disk.Read("journey") == u8"Café Ελληνικά 🚀" && disk.Read("boundary").size() == 65536,
          "Rejected managed replacement changed actual persisted contents");
    size_t slots = 0;
    for (const auto& item : std::filesystem::directory_iterator(saves / "managed-strings")) {
        Check(item.path().filename() == "slot-journey.save" || item.path().filename() == "slot-boundary.save",
              "Rejected slot input created an unexpected file");
        ++slots;
    }
    Check(slots == 2, "Managed string probe lost/added save files");
    bool badNative = false;
    try { disk.Write("journey", std::string(1, char(0xff))); }
    catch (const std::invalid_argument&) { badNative = true; }
    Check(badNative && disk.Read("journey") == u8"Café Ελληνικά 🚀",
          "Native invalid UTF-8 replaced an accepted payload");

    // Snapshot validation must reject before replacing the live script domain.
    auto invoke = [&](const char* method) {
        instance->GetScriptClass()->InvokeMethod(instance->GetManagedObject(),
            instance->GetScriptClass()->GetMethod(method, 0), nullptr);
    };
    for (const char* method : {"PoisonPrefab", "PoisonSprite", "PoisonClip"}) {
        invoke(method);
        bool rejected = false;
        try { ScriptEngine::ReloadAssembly(); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected && instance->GetManagedObject() && session.GetScene() == scene &&
              ScriptEngine::GetEntityScriptInstance(2) == instance &&
              disk.Read("journey") == u8"Café Ελληνικά 🚀",
              "Malformed snapshot path replaced the accepted domain/session/save");
        invoke("ClearSnapshot");
    }
    instance->SetFieldValue<bool>("LoadValid", true);
    session.Update(.016f);
    Check(session.GetScene() == scene, "Valid managed transition committed inside callback");
    session.Update(.016f);
    Check(session.GetScene() != scene && session.GetScene()->FindEntityByName("String target"),
          "Valid Unicode scene path was lost");
    session.Stop();
    std::cout << "PASS: complete managed UTF-16, isolated save byte limits/unchanged failures, "
                 "Unicode paths, rejected requests/instances and atomic snapshot rejection\n";
}
