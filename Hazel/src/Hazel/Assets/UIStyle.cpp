#include "hzpch.h"
#include "UIStyle.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/UTF8.h"
#include <cmath>
#include <fstream>
#include <set>
namespace Hazel
{
namespace
{
void CheckName(const std::string &value, size_t limit)
{
    if (value.empty() || value.size() > limit)
        throw std::invalid_argument("UI style name exceeds its nonempty byte limit");
    UTF8::Validate(value);
    for (auto c : value)
        if (uint8_t(c) < 32)
            throw std::invalid_argument("UI style name contains a control");
}
template <int N> void Vector(const YAML::Node &n, glm::vec<N, float> &v)
{
    if (!n.IsSequence() || n.size() != N)
        throw std::invalid_argument("Invalid UI style vector");
    for (int i = 0; i < N; ++i)
        v[i] = n[i].as<float>();
}
void Read(const YAML::Node &n, float &v)
{
    v = n.as<float>();
}
void Read(const YAML::Node &n, glm::vec2 &v)
{
    Vector(n, v);
}
void Read(const YAML::Node &n, glm::vec4 &v)
{
    Vector(n, v);
}
void Read(const YAML::Node &n, FontSelection &v)
{
    DocumentSchema::Keys(n, {"Family", "Style"}, "UI style Font");
    if (n["Family"])
        v.Family = std::filesystem::u8path(n["Family"].as<std::string>());
    if (n["Style"])
        v.Style = FontStyle(n["Style"].as<unsigned>());
    v.Validate();
}
void Read(const YAML::Node &n, SpriteSource &v)
{
    v = ReadSpriteSource(n);
}
void Read(const YAML::Node &n, UIStyleStates &v)
{
    DocumentSchema::Keys(n, {"Colors", "Images"}, "UI style States");
    if (auto a = n["Colors"])
    {
        if (!a.IsSequence() || a.size() != 5)
            throw std::invalid_argument("UI states require five colors");
        for (int i = 0; i < 5; ++i)
            Read(a[i], v.Colors[i]);
    }
    if (auto a = n["Images"])
    {
        if (!a.IsSequence() || a.size() != 5)
            throw std::invalid_argument("UI states require five images");
        for (int i = 0; i < 5; ++i)
            Read(a[i], v.Images[i]);
    }
}
void Read(const YAML::Node &n, UIStyleSlice &v)
{
    DocumentSchema::Keys(n, {"Borders", "Scale"}, "UI style Slice");
    if (n["Borders"])
        Read(n["Borders"], v.Borders);
    if (n["Scale"])
        Read(n["Scale"], v.Scale);
}
void Write(YAML::Emitter &o, float v)
{
    o << v;
}
template <int N> void Write(YAML::Emitter &o, const glm::vec<N, float> &v)
{
    o << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < N; ++i)
        o << v[i];
    o << YAML::EndSeq;
}
void Write(YAML::Emitter &o, const FontSelection &v)
{
    o << YAML::BeginMap << YAML::Key << "Family" << YAML::Value << v.Family.generic_u8string()
      << YAML::Key << "Style" << YAML::Value << unsigned(v.Style) << YAML::EndMap;
}
void Write(YAML::Emitter &o, const SpriteSource &v)
{
    o << YAML::BeginMap;
    WriteSpriteSource(o, v);
    o << YAML::EndMap;
}
void Write(YAML::Emitter &o, const UIStyleStates &v)
{
    o << YAML::BeginMap << YAML::Key << "Colors" << YAML::Value << YAML::BeginSeq;
    for (auto &c : v.Colors)
        Write(o, c);
    o << YAML::EndSeq << YAML::Key << "Images" << YAML::Value << YAML::BeginSeq;
    for (auto &c : v.Images)
        Write(o, c);
    o << YAML::EndSeq << YAML::EndMap;
}
void Write(YAML::Emitter &o, const UIStyleSlice &v)
{
    o << YAML::BeginMap << YAML::Key << "Borders" << YAML::Value;
    Write(o, v.Borders);
    o << YAML::Key << "Scale" << YAML::Value << v.Scale << YAML::EndMap;
}
void StylePath(const std::filesystem::path &p)
{
    auto text = p.generic_u8string();
    if (text.empty() || text.size() > 1024 || p.has_root_path() ||
        text.find_first_of(":\\\r\n<>\"|?*") != std::string::npos)
        throw std::invalid_argument("UI style image needs a portable asset-relative path");
    UTF8::Validate(text);
    for (auto &part : p)
        if (part == "." || part == "..")
            throw std::invalid_argument("UI style image path contains traversal");
    for (unsigned char c : text)
        if (c < 32 || c == 127)
            throw std::invalid_argument("UI style image path contains control bytes");
    auto check = p;
    check.replace_extension(".hfont");
    FontSelection{check}.Validate();
}
void Source(const SpriteSource &source)
{
    if (auto t = std::get_if<TextureSpriteSource>(&source))
    {
        if (!std::isfinite(t->TilingFactor) || t->TilingFactor < 0 || t->TilingFactor > 1000000)
            throw std::invalid_argument("Invalid style image tiling");
        if (!t->Texture.empty())
            StylePath(t->Texture);
    }
    if (auto r = std::get_if<SpriteReference>(&source))
    {
        if (r->Sheet.empty() && !r->Region)
            return;
        StylePath(r->Sheet);
        if (r->Sheet.extension() != ".hsprites" || !r->Region)
            throw std::invalid_argument("Style image requires a sheet and stable region ID");
    }
}
} // namespace
const char *UIStyleGroupName(UIStyleGroup group)
{
    static const char *names[] = {
#define ITEM(n) #n,
        HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
    };
    if (unsigned(group) >= unsigned(UIStyleGroup::Count))
        throw std::invalid_argument("Invalid style group");
    return names[unsigned(group)];
}
bool UIStyleValues::operator==(const UIStyleValues &b) const
{
    return true
#define ITEM(n) &&n == b.n
        HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
            ;
}
void CopyUIStyleGroups(UIStyleValues &target, const UIStyleValues &source, uint32_t groups)
{
    if (groups & ~UIStyleAll)
        throw std::invalid_argument("Invalid override mask");
#define ITEM(n)                                                                                    \
    if (groups & UIStyleBit(UIStyleGroup::n))                                                      \
        target.n = source.n;
    HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
}
void UIStyleValues::Validate() const
{
    Font.Validate();
    if (!std::isfinite(Size) || Size <= 0 || Size > 512)
        throw std::invalid_argument("UI style size must be 0 < size <= 512");
    auto finite = [](auto v) {
        for (int i = 0; i < v.length(); ++i)
            if (!std::isfinite(v[i]) || std::abs(v[i]) > 1000000)
                throw std::invalid_argument("UI style has nonfinite/excessive values");
    };
    if (Spacing.x < -8 || Spacing.x > 8 || Spacing.y < 0 || Spacing.y > 8)
        throw std::invalid_argument(
            "Style tracking needs -8..8 and additional line spacing 0..8 font heights");
    finite(Spacing);
    finite(Padding);
    finite(Slice.Borders);
    for (int i = 0; i < 4; ++i)
        if (Padding[i] < 0 || Slice.Borders[i] < 0)
            throw std::invalid_argument("UI style padding/borders cannot be negative");
    if (!std::isfinite(Slice.Scale) || Slice.Scale <= 0 || Slice.Scale > 10000)
        throw std::invalid_argument("Invalid slice scale");
    for (auto c : {Foreground, Background, Focus, Selected, Editing, Caret, Selection})
    {
        finite(c);
        for (int i = 0; i < 4; ++i)
            if (c[i] < 0 || c[i] > 1)
                throw std::invalid_argument("Style appearance colors need 0..1");
    }
    for (auto c : States.Colors)
    {
        finite(c);
        for (int i = 0; i < 4; ++i)
            if (c[i] < 0 || c[i] > 4)
                throw std::invalid_argument("State multipliers need 0..4");
    }
    Source(Image);
    for (auto &s : States.Images)
        Source(s);
}
void UIStyleSelection::Validate() const
{
    CheckName(Variant, 64);
    if (!Asset.empty())
    {
        if (Asset.extension() != ".hstyle")
            throw std::invalid_argument("UI style requires .hstyle (empty = engine default)");
        auto check = Asset;
        check.replace_extension(".hfont");
        FontSelection{check}.Validate();
    }
}
void UIStyleDefinition::Validate() const
{
    CheckName(this->Name, 128);
    if (Variants.empty() || Variants.size() > 16)
        throw std::invalid_argument("Style requires 1..16 flat variants");
    std::set<std::string> seen;
    for (auto &v : Variants)
    {
        ::Hazel::UIStyleSelection{{}, v.Name}.Validate();
        if (!seen.insert(v.Name).second)
            throw std::invalid_argument("Duplicate style variant: " + v.Name);
        v.Values.Validate();
        auto portable = [](const SpriteSource &s) {
            if (auto t = std::get_if<TextureSpriteSource>(&s); t && t->Resource)
                throw std::invalid_argument("Style assets cannot retain native texture owners; "
                                            "select portable assets before extraction");
        };
        portable(v.Values.Image);
        for (auto &image : v.Values.States.Images)
            portable(image);
    }
}
const UIStyleValues &UIStyleDefinition::Find(const std::string &name) const
{
    for (auto &v : Variants)
        if (v.Name == name)
            return v.Values;
    throw std::invalid_argument("Missing UI style variant: " + name);
}
UIStyleValues ReadUIStyleValues(const YAML::Node &node)
{
    DocumentSchema::Keys(node,
                         {
#define ITEM(n) #n,
                             HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
                         },
                         "UI style values");
    UIStyleValues v;
    VisitUIStyle(v, [&](auto group, auto &field) {
        if (auto n = node[UIStyleGroupName(group)])
            Read(n, field);
    });
    v.Validate();
    return v;
}
void WriteUIStyleValues(YAML::Emitter &o, const UIStyleValues &v)
{
    v.Validate();
    o << YAML::BeginMap;
    VisitUIStyle(v, [&](auto group, const auto &field) {
        o << YAML::Key << UIStyleGroupName(group) << YAML::Value;
        Write(o, field);
    });
    o << YAML::EndMap;
}
UIStyleDefinition ReadUIStyleText(const std::string &text)
{
    if (text.size() > 65536)
        throw std::invalid_argument("Style exceeds 64 KiB");
    auto root = YAML::Load(text);
    DocumentSchema::Structure(root);
    DocumentSchema::Keys(root, {"UIStyle"}, "root");
    auto n = root["UIStyle"];
    DocumentSchema::Keys(n, {"Version", "Name", "Variants"}, "UIStyle");
    if (n["Version"].as<unsigned>() != 1)
        throw std::invalid_argument("Unsupported UIStyle.Version");
    UIStyleDefinition d;
    d.Name = n["Name"].as<std::string>();
    auto variants = n["Variants"];
    if (!variants.IsSequence() || variants.size() > 16)
        throw std::invalid_argument("Style needs <=16 variants");
    d.Variants.clear();
    for (auto v : variants)
    {
        DocumentSchema::Keys(v, {"Name", "Values"}, "Variant");
        d.Variants.push_back({v["Name"].as<std::string>(), ReadUIStyleValues(v["Values"])});
    }
    d.Validate();
    return d;
}
UIStyleDefinition ReadUIStyle(const std::filesystem::path &file)
{
    std::ifstream f(file, std::ios::binary | std::ios::ate);
    auto size = f ? f.tellg() : std::streampos(-1);
    if (size < 0 || size > 65536)
        throw std::runtime_error("Missing/oversized UI style: " + file.generic_u8string());
    std::string bytes(size_t(size), '\0');
    f.seekg(0);
    if (!bytes.empty() && !f.read(bytes.data(), bytes.size()))
        throw std::runtime_error("Cannot read UI style");
    return ReadUIStyleText(bytes);
}
std::string WriteUIStyleText(const UIStyleDefinition &d)
{
    d.Validate();
    YAML::Emitter o;
    o.SetFloatPrecision(9);
    o << YAML::BeginMap << YAML::Key << "UIStyle" << YAML::Value << YAML::BeginMap << YAML::Key
      << "Version" << YAML::Value << 1 << YAML::Key << "Name" << YAML::Value << d.Name << YAML::Key
      << "Variants" << YAML::Value << YAML::BeginSeq;
    for (auto &v : d.Variants)
    {
        o << YAML::BeginMap << YAML::Key << "Name" << YAML::Value << v.Name << YAML::Key << "Values"
          << YAML::Value;
        WriteUIStyleValues(o, v.Values);
        o << YAML::EndMap;
    }
    o << YAML::EndSeq << YAML::EndMap << YAML::EndMap;
    std::string text = o.c_str();
    if (text.size() > 65536)
        throw std::invalid_argument("Style exceeds 64 KiB");
    return text;
}
const UIStyleDefinition &DefaultUIStyle()
{
    static const auto definition = ReadUIStyle(Resources::Resolve("UI/Default.hstyle"));
    return definition;
}
std::vector<std::filesystem::path> AuditUIStyle(const std::filesystem::path &root,
                                                const UIStyleDefinition &d)
{
    d.Validate();
    std::set<std::filesystem::path> files;
    for (auto &v : d.Variants)
    {
        try
        {
            if (!v.Values.Font.Family.empty())
            {
                if (files.insert(v.Values.Font.Family).second)
                    for (auto &p : AuditFontFamily(root, v.Values.Font.Family))
                        files.insert(p);
            }
            auto image = [&](const SpriteSource &s) {
                if (auto t = std::get_if<TextureSpriteSource>(&s))
                {
                    if (!t->Texture.empty())
                    {
                        Texture2D::ReadImage(Project::ResolveOwnedAsset(root, t->Texture));
                        files.insert(t->Texture);
                    }
                }
                if (auto r = std::get_if<SpriteReference>(&s))
                {
                    auto sheet = ReadSpriteSheet(Project::ResolveOwnedAsset(root, r->Sheet));
                    sheet.Region(r->Region);
                    auto pixels =
                        Texture2D::ReadImage(Project::ResolveOwnedAsset(root, sheet.Texture));
                    if (pixels.Width != sheet.Sampling.Width ||
                        pixels.Height != sheet.Sampling.Height)
                        throw std::invalid_argument("Style sheet texture dimensions changed");
                    files.insert(r->Sheet);
                    files.insert(sheet.Texture);
                }
            };
            image(v.Values.Image);
            for (auto &s : v.Values.States.Images)
                image(s);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Style '" + d.Name + "' variant '" + v.Name +
                                     "': " + e.what());
        }
    }
    return {files.begin(), files.end()};
}
} // namespace Hazel
