#include "Hazel/Assets/UIStyleDocument.h"
#include <limits>
static void UIStyleAssetChecks()
{
    using namespace Hazel;
    namespace fs = std::filesystem;
    const auto root =
        fs::temp_directory_path() / ("hazel-style-" + std::to_string(uint64_t(UUID())));
    fs::create_directories(root);
    struct Cleanup
    {
        fs::path Root;
        ~Cleanup()
        {
            std::error_code ec;
            fs::remove_all(Root, ec);
        }
    } cleanup{root};
    auto reject = [&](auto fn) {
        bool failed = false;
        try
        {
            fn();
        }
        catch (const std::exception &)
        {
            failed = true;
        }
        Check(failed, "Invalid style input accepted");
    };
    UIStyleDefinition definition;
    definition.Name = "Shared settings";
    definition.Variants[0].Values.Padding = glm::vec4(12);
    definition.Variants.push_back({"Quiet", {}});
    auto text = WriteUIStyleText(definition);
    auto loaded = ReadUIStyleText(text);
    Check(loaded.Find("Control") == definition.Find("Control"),
          "Typed style roundtrip changed values");
    reject([&] {
        ReadUIStyleText("UIStyle: {Version: 1, Name: X, Variants: [{Name: A, Values: {}}, {Name: "
                        "A, Values: {}}]}");
    });
    reject([&] { ReadUIStyleText("UIStyle: {Version: 1, Name: X, Selectors: [], Variants: []}"); });
    reject([&] { UIStyleSelection{"../bad.hstyle"}.Validate(); });
    reject([&] { UIStyleSelection{"C:/bad.hstyle"}.Validate(); });
    reject([&] { ReadUIStyleText(std::string(65537, 'x')); });
    reject([&] {
        auto invalid = definition;
        invalid.Variants[0].Values.Size = std::numeric_limits<float>::infinity();
        invalid.Validate();
    });
    auto effective = definition.Find("Control"), local = effective;
    local.Padding = {};
    local.Foreground = {0, 0, 0, 0};
    CopyUIStyleGroups(effective, local,
                      UIStyleBit(UIStyleGroup::Padding) | UIStyleBit(UIStyleGroup::Foreground));
    Check(effective.Padding == glm::vec4(0) && effective.Foreground == glm::vec4(0),
          "Zero override was treated as inheritance");
    auto assets = CreateRef<ProjectAssets>(root);
    UIStyleDocument doc(assets);
    doc.Create("Shared.hstyle", definition);
    auto first = assets->Style("Shared.hstyle");
    Check(first.Accepted && first.Error.empty(), "Style cache did not accept authored definition");
    Check(assets->Style("Shared.hstyle").Accepted == first.Accepted,
          "Unchanged style reread/rebuilt accepted metadata");
    doc.Draft().Variants[0].Values.Size = 32;
    doc.Changed();
    doc.Save();
    auto next = assets->Style("Shared.hstyle");
    Check(next.Accepted != first.Accepted && next.Accepted->Find("Control").Size == 32 &&
              first.Accepted->Find("Control").Size == 24,
          "Accepted style was mutated underneath a pinned user");
    Check(assets->StyleLiveVersions() == 2 && assets->StyleLiveBytes() > assets->StyleBytes(),
          "Pinned old style version was not separately accounted");
    auto saved = FileDocument::Read(root / "Shared.hstyle");
    // Model Windows text-mode LF -> CRLF conversion on every platform. A
    // newline-only foreign edit must remain a conflict, never be normalized.
    std::string translated;
    for (char c : saved) {
        if (c == '\n') translated += '\r';
        translated += c;
    }
    Check(translated != saved, "Style fixture has no line-ending control");
    FileDocument exact;
    exact.Open(root / "Shared.hstyle");
    FileSystem::WriteFileAtomically(root / "Shared.hstyle", [&](auto& out) { out << translated; });
    reject([&] { exact.Check(); });
    Check(FileDocument::Read(root / "Shared.hstyle") == translated,
          "Conflict detection rewrote a newline-only foreign edit");
    FileSystem::WriteFileAtomically(root / "Shared.hstyle", [&](auto& out) { out << saved; });
    exact.Check();
    {
        std::ofstream out(root / "Shared.hstyle");
        out << "not a style";
    }
    assets->Reload("Shared.hstyle");
    auto broken = assets->Style("Shared.hstyle");
    Check(broken.Accepted == next.Accepted && !broken.Error.empty(),
          "Malformed replacement discarded accepted appearance");
    Check(assets->Style("Shared.hstyle").Accepted == broken.Accepted,
          "Failed style load retried during lookup");
    doc.Draft().Name = "Conflict draft";
    doc.Changed();
    reject([&] { doc.Save(); });
    Check(doc.Dirty() && FileDocument::Read(root / "Shared.hstyle") == "not a style",
          "Conflict overwrote foreign bytes or discarded draft");
    {
        std::ofstream out(root / "Shared.hstyle", std::ios::binary);
        out << saved;
    }
    Check(FileDocument::Read(root / "Shared.hstyle") == saved,
          "Style repair changed the accepted byte baseline");
    assets->Reload("Shared.hstyle");
    Check(assets->Style("Shared.hstyle").Error.empty(), "Repaired style could not reload");
    UIStyleDocument reopened(assets);
    reopened.Open("Shared.hstyle");
    Check(reopened.Draft().Variants[0].Values.Size == 32 && !reopened.Dirty(),
          "Style reopen/save point failed");
    auto dependency = definition;
    dependency.Variants[0].Values.Font.Family = "missing.hfont";
    reject([&] { AuditUIStyle(root, dependency); });
    std::vector<Ref<const UIStyleDefinition>> pins;
    for (size_t i = 1; i < ProjectAssets::MaxStyleEntries; ++i)
    {
        auto name = "Style" + std::to_string(i) + ".hstyle";
        std::ofstream(root / name) << text;
        auto result = assets->Style(name);
        Check(bool(result.Accepted), "Small bounded style cache fixture rejected prematurely");
        pins.push_back(result.Accepted);
    }
    auto full = assets->Style("Overflow.hstyle");
    Check(!full.Accepted && !full.Error.empty() && assets->StyleCount() == 64,
          "Style retention limit failed admission");
    pins.clear();
    assets->Refresh();
    Check(assets->StyleCount() < 64, "Explicit refresh did not retire unpinned style metadata");
    first.Accepted.reset();
    next.Accepted.reset();
    broken.Accepted.reset();
    std::vector<Ref<const UIStyleDefinition>> versions;
    bool denied = false;
    for (size_t i = 0; i < ProjectAssets::MaxStyleVersions + 1; ++i)
    {
        auto d = definition;
        d.Name = "Version " + std::to_string(i);
        std::ofstream(root / "Shared.hstyle") << WriteUIStyleText(d);
        assets->Reload("Shared.hstyle");
        auto r = assets->Style("Shared.hstyle");
        if (!r.Error.empty())
        {
            denied = true;
            break;
        }
        versions.push_back(r.Accepted);
    }
    Check(denied && assets->StyleLiveVersions() <= ProjectAssets::MaxStyleVersions &&
              assets->StyleLiveBytes() <= ProjectAssets::MaxStyleLiveBytes,
          "Pinned metadata pressure silently exceeded its admission limits");
    versions.clear();
    assets->Reload("Shared.hstyle");
    Check(assets->Style("Shared.hstyle").Error.empty(),
          "Style admission did not recover after old version retirement");
    std::cout
        << "PASS: typed style codec/zero presence, immutable accepted versions, bounded retention, "
           "malformed reload/repair, dependency failure and atomic document conflicts\n";
}
