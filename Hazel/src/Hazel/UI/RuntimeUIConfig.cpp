#include "hzpch.h"
#include "RuntimeUIConfig.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Entity.h"
#include <cmath>
#include <limits>
#include <set>
namespace Hazel
{
namespace
{
void Keys(const YAML::Node &n, std::initializer_list<const char *> fields)
{
    if (!n.IsMap())
        throw std::runtime_error("UI component must be a map");
    std::set<std::string> allowed;
    for (auto f : fields)
        allowed.insert(f);
    std::set<std::string> seen;
    for (auto f : n)
    {
        auto key = f.first.as<std::string>();
        if (!allowed.count(key) || !seen.insert(key).second)
            throw std::runtime_error("Unknown/duplicate UI field: " + key);
    }
}
template <int N> glm::vec<N, float, glm::defaultp> Vector(const YAML::Node &n)
{
    if (!n.IsSequence() || n.size() != N)
        throw std::runtime_error("Invalid UI vector");
    glm::vec<N, float, glm::defaultp> v;
    for (int i = 0; i < N; i++)
    {
        v[i] = n[i].as<float>();
        if (!std::isfinite(v[i]))
            throw std::runtime_error("UI values must be finite");
    }
    return v;
}
template <int N> void Write(YAML::Emitter &out, const glm::vec<N, float, glm::defaultp> &v)
{
    out << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < N; i++)
        out << v[i];
    out << YAML::EndSeq;
}
void Write(YAML::Emitter &out, const std::filesystem::path &p)
{
    out << p.generic_u8string();
}
void Write(YAML::Emitter &out, GameUI::Axis v)
{
    out << int(v);
}
void Write(YAML::Emitter &out, GameUI::Align v)
{
    out << int(v);
}
template <class T> void Write(YAML::Emitter &out, const T &v)
{
    out << v;
}
void Positive(float v, bool zero = true)
{
    if (!std::isfinite(v) || v < 0 || (!zero && v == 0))
        throw std::runtime_error("Invalid UI dimension");
}
void OwnedFont(const std::filesystem::path &p)
{
    if (p.empty())
        return;
    auto n = Project::NormalizeAssetPath(p);
    if (n.has_root_path() || (n.extension() != ".ttf" && n.extension() != ".otf") ||
        p.generic_u8string().find(':') != std::string::npos)
        throw std::runtime_error("Font must be a project-relative TTF/OTF");
    for (auto part : n)
        if (part == "..")
            throw std::runtime_error("Font escapes project");
}
GameUI::Align ReadAlign(const YAML::Node &n)
{
    int v = n.as<int>();
    if (v < 0 || v > 3)
        throw std::runtime_error("Invalid UI alignment");
    return GameUI::Align(v);
}
} // namespace
UICanvasComponent ReadUICanvasComponent(const YAML::Node &n)
{
    Keys(n, {"ReferenceSize", "SafeInsets", "SortOrder", "Visible", "Enabled", "Modal"});
    UICanvasComponent v;
    if (n["ReferenceSize"])
        v.ReferenceSize = Vector<2>(n["ReferenceSize"]);
    if (n["SafeInsets"])
        v.SafeInsets = Vector<4>(n["SafeInsets"]);
    if (n["SortOrder"])
        v.SortOrder = n["SortOrder"].as<int>();
    if (n["Visible"])
        v.Visible = n["Visible"].as<bool>();
    if (n["Enabled"])
        v.Enabled = n["Enabled"].as<bool>();
    if (n["Modal"])
        v.Modal = n["Modal"].as<bool>();
    GameUI::FitCanvas({1280, 720}, v);
    return v;
}
static void Write(YAML::Emitter &out, const UICanvasComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "ReferenceSize" << YAML::Value;
    Write(out, v.ReferenceSize);
    out << YAML::Key << "SafeInsets" << YAML::Value;
    Write(out, v.SafeInsets);
    out << YAML::Key << "SortOrder" << YAML::Value;
    Write(out, v.SortOrder);
    out << YAML::Key << "Visible" << YAML::Value;
    Write(out, v.Visible);
    out << YAML::Key << "Enabled" << YAML::Value;
    Write(out, v.Enabled);
    out << YAML::Key << "Modal" << YAML::Value;
    Write(out, v.Modal);
    out << YAML::EndMap;
}
UIRectComponent ReadUIRectComponent(const YAML::Node &n)
{
    Keys(n, {"AnchorMin", "AnchorMax", "Offset", "SizeDelta", "Pivot", "MinimumSize", "Margin",
             "Padding", "Visible", "Enabled", "ClipChildren", "Flex", "LayoutVersion",
             "WidthMode", "HeightMode", "FixedSize", "MaximumSize", "HasMaximum"});
    UIRectComponent v;
    if(n["LayoutVersion"])v.LayoutVersion=n["LayoutVersion"].as<unsigned>();
    if(v.LayoutVersion==1 && (n["WidthMode"]||n["HeightMode"]||n["FixedSize"]||n["MaximumSize"]||n["HasMaximum"]))
        throw std::runtime_error("Measured sizing fields require LayoutVersion 2");
    if(n["WidthMode"])v.WidthMode=GameUI::SizeMode(n["WidthMode"].as<int>());
    if(n["HeightMode"])v.HeightMode=GameUI::SizeMode(n["HeightMode"].as<int>());
    if(n["FixedSize"])v.FixedSize=Vector<2>(n["FixedSize"]);
    if(n["MaximumSize"])v.MaximumSize=Vector<2>(n["MaximumSize"]);
    if(auto maximum=n["HasMaximum"]){
        if(!maximum.IsSequence()||maximum.size()!=2)throw std::runtime_error("HasMaximum requires two booleans");
        v.HasMaximum={maximum[0].as<bool>(),maximum[1].as<bool>()};
    }
    if (n["AnchorMin"])
        v.AnchorMin = Vector<2>(n["AnchorMin"]);
    if (n["AnchorMax"])
        v.AnchorMax = Vector<2>(n["AnchorMax"]);
    if (n["Offset"])
        v.Offset = Vector<2>(n["Offset"]);
    if (n["SizeDelta"])
        v.SizeDelta = Vector<2>(n["SizeDelta"]);
    if (n["Pivot"])
        v.Pivot = Vector<2>(n["Pivot"]);
    if (n["MinimumSize"])
        v.MinimumSize = Vector<2>(n["MinimumSize"]);
    if (n["Margin"])
        v.Margin = Vector<4>(n["Margin"]);
    if (n["Padding"])
        v.Padding = Vector<4>(n["Padding"]);
    if (n["Visible"])
        v.Visible = n["Visible"].as<bool>();
    if (n["Enabled"])
        v.Enabled = n["Enabled"].as<bool>();
    if (n["ClipChildren"])
        v.ClipChildren = n["ClipChildren"].as<bool>();
    if (n["Flex"])
        v.Flex = n["Flex"].as<float>();
    GameUI::Validate(v);
    Positive(v.Flex);
    return v;
}
static void Write(YAML::Emitter &out, const UIRectComponent &v)
{
    out << YAML::BeginMap;
    if(v.LayoutVersion!=1){
        out << YAML::Key << "LayoutVersion" << YAML::Value << v.LayoutVersion;
        out << YAML::Key << "WidthMode" << YAML::Value << int(v.WidthMode);
        out << YAML::Key << "HeightMode" << YAML::Value << int(v.HeightMode);
        out << YAML::Key << "FixedSize" << YAML::Value;Write(out,v.FixedSize);
        out << YAML::Key << "MaximumSize" << YAML::Value;Write(out,v.MaximumSize);
        out << YAML::Key << "HasMaximum" << YAML::Value << YAML::Flow << YAML::BeginSeq << bool(v.HasMaximum.x) << bool(v.HasMaximum.y) << YAML::EndSeq;
    }
    out << YAML::Key << "AnchorMin" << YAML::Value;
    Write(out, v.AnchorMin);
    out << YAML::Key << "AnchorMax" << YAML::Value;
    Write(out, v.AnchorMax);
    out << YAML::Key << "Offset" << YAML::Value;
    Write(out, v.Offset);
    out << YAML::Key << "SizeDelta" << YAML::Value;
    Write(out, v.SizeDelta);
    out << YAML::Key << "Pivot" << YAML::Value;
    Write(out, v.Pivot);
    out << YAML::Key << "MinimumSize" << YAML::Value;
    Write(out, v.MinimumSize);
    out << YAML::Key << "Margin" << YAML::Value;
    Write(out, v.Margin);
    out << YAML::Key << "Padding" << YAML::Value;
    Write(out, v.Padding);
    out << YAML::Key << "Visible" << YAML::Value;
    Write(out, v.Visible);
    out << YAML::Key << "Enabled" << YAML::Value;
    Write(out, v.Enabled);
    out << YAML::Key << "ClipChildren" << YAML::Value;
    Write(out, v.ClipChildren);
    out << YAML::Key << "Flex" << YAML::Value;
    Write(out, v.Flex);
    out << YAML::EndMap;
}
UILayoutComponent ReadUILayoutComponent(const YAML::Node &n)
{
    Keys(n, {"Direction", "Main", "Cross", "Gap"});
    UILayoutComponent v;
    if (n["Direction"])
        v.Direction = GameUI::Axis(n["Direction"].as<int>());
    if (n["Main"])
        v.Main = ReadAlign(n["Main"]);
    if (n["Cross"])
        v.Cross = ReadAlign(n["Cross"]);
    if (n["Gap"])
        v.Gap = n["Gap"].as<float>();
    if (int(v.Direction) < 0 || int(v.Direction) > 1)
        throw std::runtime_error("Invalid flow direction");
    if(v.Main>GameUI::Align::End)throw std::runtime_error("Flow main alignment must be Start, Center or End");
    Positive(v.Gap);
    return v;
}
static void Write(YAML::Emitter &out, const UILayoutComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Direction" << YAML::Value;
    Write(out, v.Direction);
    out << YAML::Key << "Main" << YAML::Value;
    Write(out, v.Main);
    out << YAML::Key << "Cross" << YAML::Value;
    Write(out, v.Cross);
    out << YAML::Key << "Gap" << YAML::Value;
    Write(out, v.Gap);
    out << YAML::EndMap;
}
UITextStyleComponent ReadUITextStyleComponent(const YAML::Node &n)
{
    Keys(n, {"FontPath", "Size", "Horizontal", "Vertical", "Wrap"});
    UITextStyleComponent v;
    if (n["FontPath"])
        v.FontPath = std::filesystem::u8path(n["FontPath"].as<std::string>());
    if (n["Size"])
        v.Size = n["Size"].as<float>();
    if (n["Horizontal"])
        v.Horizontal = ReadAlign(n["Horizontal"]);
    if (n["Vertical"])
        v.Vertical = ReadAlign(n["Vertical"]);
    if (n["Wrap"])
        v.Wrap = n["Wrap"].as<bool>();
    if(v.Horizontal>GameUI::Align::End||v.Vertical>GameUI::Align::End)throw std::runtime_error("Text alignment must be Start, Center or End");
    Positive(v.Size, false);
    if (v.Size > 512)
        throw std::runtime_error("UI font size exceeds 512");
    OwnedFont(v.FontPath);
    return v;
}
static void Write(YAML::Emitter &out, const UITextStyleComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "FontPath" << YAML::Value;
    Write(out, v.FontPath);
    out << YAML::Key << "Size" << YAML::Value;
    Write(out, v.Size);
    out << YAML::Key << "Horizontal" << YAML::Value;
    Write(out, v.Horizontal);
    out << YAML::Key << "Vertical" << YAML::Value;
    Write(out, v.Vertical);
    out << YAML::Key << "Wrap" << YAML::Value;
    Write(out, v.Wrap);
    out << YAML::EndMap;
}
UINineSliceComponent ReadUINineSliceComponent(const YAML::Node &n)
{
    Keys(n, {"Borders", "BorderScale"});
    UINineSliceComponent v;
    if (n["Borders"])
        v.Borders = Vector<4>(n["Borders"]);
    if (n["BorderScale"])
        v.BorderScale = n["BorderScale"].as<float>();
    Positive(v.BorderScale, false);
    for (int i = 0; i < 4; i++)
        Positive(v.Borders[i]);
    return v;
}
static void Write(YAML::Emitter &out, const UINineSliceComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Borders" << YAML::Value;
    Write(out, v.Borders);
    out << YAML::Key << "BorderScale" << YAML::Value;
    Write(out, v.BorderScale);
    out << YAML::EndMap;
}
UIButtonComponent ReadUIButtonComponent(const YAML::Node &n)
{
    Keys(n, {"Action", "TabOrder", "Interactable", "Colors", "Images"});
    UIButtonComponent v;
    if (n["Action"])
        v.Action = n["Action"].as<std::string>();
    if (n["TabOrder"])
        v.TabOrder = n["TabOrder"].as<int>();
    if (n["Interactable"])
        v.Interactable = n["Interactable"].as<bool>();
    if (v.Action.size() > 128 || v.Action.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid UI action");
    if (auto nColors = n["Colors"])
    {
        if (!nColors.IsSequence() || nColors.size() != 5)
            throw std::runtime_error("Expected five button colors");
        for (int i = 0; i < 5; i++)
            v.Colors[i] = Vector<4>(nColors[i]);
    }
    if (auto images = n["Images"])
    {
        if (!images.IsSequence() || images.size() != 5)
            throw std::runtime_error("Expected five button images");
        for (int i = 0; i < 5; i++)
        {
            Keys(images[i], {"Source"});
            v.Images[i] = ReadSpriteSource(images[i]);
        }
    }
    return v;
}
static void Write(YAML::Emitter &out, const UIButtonComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Action" << YAML::Value;
    Write(out, v.Action);
    out << YAML::Key << "TabOrder" << YAML::Value;
    Write(out, v.TabOrder);
    out << YAML::Key << "Interactable" << YAML::Value;
    Write(out, v.Interactable);
    out << YAML::Key << "Colors" << YAML::Value << YAML::BeginSeq;
    for (auto c : v.Colors)
        Write(out, c);
    out << YAML::EndSeq;
    out << YAML::Key << "Images" << YAML::Value << YAML::BeginSeq;
    for (auto s : v.Images)
    {
        out << YAML::BeginMap;
        WriteSpriteSource(out, s);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
}
void ValidateUIConfiguration(const YAML::Node &node)
{
    if (auto n = node["UICanvasComponent"])
        ReadUICanvasComponent(n);
    if (auto n = node["UIRectComponent"])
        ReadUIRectComponent(n);
    if (auto n = node["UILayoutComponent"])
        ReadUILayoutComponent(n);
    if (auto n = node["UITextStyleComponent"])
        ReadUITextStyleComponent(n);
    if (auto n = node["UINineSliceComponent"])
        ReadUINineSliceComponent(n);
    if (auto n = node["UIButtonComponent"])
        ReadUIButtonComponent(n);
}
void ReadUIComponents(const YAML::Node &node, Entity entity)
{
    if (auto n = node["UICanvasComponent"])
        entity.AddComponent<UICanvasComponent>(ReadUICanvasComponent(n));
    if (auto n = node["UIRectComponent"])
        entity.AddComponent<UIRectComponent>(ReadUIRectComponent(n));
    if (auto n = node["UILayoutComponent"])
        entity.AddComponent<UILayoutComponent>(ReadUILayoutComponent(n));
    if (auto n = node["UITextStyleComponent"])
        entity.AddComponent<UITextStyleComponent>(ReadUITextStyleComponent(n));
    if (auto n = node["UINineSliceComponent"])
        entity.AddComponent<UINineSliceComponent>(ReadUINineSliceComponent(n));
    if (auto n = node["UIButtonComponent"])
        entity.AddComponent<UIButtonComponent>(ReadUIButtonComponent(n));
}
void WriteUIComponents(YAML::Emitter &out, Entity entity)
{
    if (entity.HasComponent<UICanvasComponent>())
    {
        out << YAML::Key << "UICanvasComponent" << YAML::Value;
        Write(out, entity.GetComponent<UICanvasComponent>());
    }
    if (entity.HasComponent<UIRectComponent>())
    {
        out << YAML::Key << "UIRectComponent" << YAML::Value;
        Write(out, entity.GetComponent<UIRectComponent>());
    }
    if (entity.HasComponent<UILayoutComponent>())
    {
        out << YAML::Key << "UILayoutComponent" << YAML::Value;
        Write(out, entity.GetComponent<UILayoutComponent>());
    }
    if (entity.HasComponent<UITextStyleComponent>())
    {
        out << YAML::Key << "UITextStyleComponent" << YAML::Value;
        Write(out, entity.GetComponent<UITextStyleComponent>());
    }
    if (entity.HasComponent<UINineSliceComponent>())
    {
        out << YAML::Key << "UINineSliceComponent" << YAML::Value;
        Write(out, entity.GetComponent<UINineSliceComponent>());
    }
    if (entity.HasComponent<UIButtonComponent>())
    {
        out << YAML::Key << "UIButtonComponent" << YAML::Value;
        Write(out, entity.GetComponent<UIButtonComponent>());
    }
}
std::string UIAuthoredState(Entity entity)
{
    if (!entity.HasComponent<UICanvasComponent>() && !entity.HasComponent<UIRectComponent>() &&
        !entity.HasComponent<UILayoutComponent>() && !entity.HasComponent<UITextStyleComponent>() &&
        !entity.HasComponent<UINineSliceComponent>() && !entity.HasComponent<UIButtonComponent>())
        return {};
    YAML::Emitter out;
    out.SetFloatPrecision(std::numeric_limits<float>::max_digits10);
    out << YAML::BeginMap;
    WriteUIComponents(out, entity);
    out << YAML::EndMap;
    return out.c_str();
}
} // namespace Hazel
