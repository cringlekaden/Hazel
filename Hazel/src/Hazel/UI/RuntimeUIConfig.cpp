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
    if (v < 0 || v > 2)
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
    if(!n["LayoutVersion"] || n["LayoutVersion"].as<unsigned>()!=GameUI::RectLayout::SchemaVersion)
        throw std::runtime_error("Unsupported UI Rect schema: expected LayoutVersion 3. Repository UI uses the canonical measured model; recover obsolete documents from their Git checkpoint.");
    Keys(n, {"LayoutVersion","Anchor","Offset","Pivot","WidthMode","HeightMode","FixedSize","MinimumSize","MaximumSize","HasMaximum","Margin","Padding","Visible","Enabled","ClipChildren","Flex","GridRow","GridColumn","RowSpan","ColumnSpan","ImageFit","ImageAlignment"});
    UIRectComponent v;
    if(n["WidthMode"])v.WidthMode=GameUI::SizeMode(n["WidthMode"].as<int>());
    if(n["HeightMode"])v.HeightMode=GameUI::SizeMode(n["HeightMode"].as<int>());
    if(n["FixedSize"])v.FixedSize=Vector<2>(n["FixedSize"]);
    if(n["MaximumSize"])v.MaximumSize=Vector<2>(n["MaximumSize"]);
    if(auto m=n["HasMaximum"]){if(!m.IsSequence()||m.size()!=2)throw std::runtime_error("HasMaximum needs two flags");v.HasMaximum={m[0].as<bool>(),m[1].as<bool>()};}
    if(n["Anchor"])v.Anchor=Vector<2>(n["Anchor"]);
    if(n["GridRow"])v.GridRow=n["GridRow"].as<int>();
    if(n["GridColumn"])v.GridColumn=n["GridColumn"].as<int>();
    if(n["RowSpan"])v.RowSpan=n["RowSpan"].as<unsigned>();
    if(n["ColumnSpan"])v.ColumnSpan=n["ColumnSpan"].as<unsigned>();
    if(n["ImageFit"])v.Fit=GameUI::ImageFit(n["ImageFit"].as<int>());
    if(n["ImageAlignment"])v.ImageAlignment=Vector<2>(n["ImageAlignment"]);
    GameUI::FitImage({}, {1,1},v.Fit,v.ImageAlignment);
    for(int a=0;a<2;++a)if(v.ImageAlignment[a]<0||v.ImageAlignment[a]>1)throw std::runtime_error("Image alignment must be normalized");
    if (n["Offset"])
        v.Offset = Vector<2>(n["Offset"]);
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
    out << YAML::Key << "LayoutVersion" << YAML::Value << GameUI::RectLayout::SchemaVersion;
    out << YAML::Key << "WidthMode" << YAML::Value << int(v.WidthMode);
    out << YAML::Key << "HeightMode" << YAML::Value << int(v.HeightMode);
    out << YAML::Key << "FixedSize" << YAML::Value;Write(out,v.FixedSize);
    out << YAML::Key << "MaximumSize" << YAML::Value;Write(out,v.MaximumSize);
    out << YAML::Key << "HasMaximum" << YAML::Value << YAML::Flow << YAML::BeginSeq << bool(v.HasMaximum.x) << bool(v.HasMaximum.y) << YAML::EndSeq;
    out << YAML::Key << "Anchor" << YAML::Value;Write(out,v.Anchor);
    out << YAML::Key << "GridRow" << YAML::Value << v.GridRow;
    out << YAML::Key << "GridColumn" << YAML::Value << v.GridColumn;
    out << YAML::Key << "RowSpan" << YAML::Value << v.RowSpan;
    out << YAML::Key << "ColumnSpan" << YAML::Value << v.ColumnSpan;
    out << YAML::Key << "ImageFit" << YAML::Value << int(v.Fit);
    out << YAML::Key << "ImageAlignment" << YAML::Value;Write(out,v.ImageAlignment);
    out << YAML::Key << "Offset" << YAML::Value;
    Write(out, v.Offset);
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
static std::vector<GameUI::Track> ReadTracks(const YAML::Node &n)
{
    if(!n.IsSequence() || n.size()==0 || n.size()>64)throw std::runtime_error("Track list requires 1..64 entries");
    std::vector<GameUI::Track> result;
    for(auto item:n){
        Keys(item,{"Mode","Value","Minimum","Maximum","HasMaximum"});GameUI::Track t;
        if(item["Mode"])t.Mode=GameUI::TrackMode(item["Mode"].as<int>());
        if(item["Value"])t.Value=item["Value"].as<float>();
        if(item["Minimum"])t.Minimum=item["Minimum"].as<float>();
        if(item["Maximum"])t.Maximum=item["Maximum"].as<float>();
        if(item["HasMaximum"])t.HasMaximum=item["HasMaximum"].as<bool>();
        GameUI::Validate(t);result.push_back(t);
    }return result;
}
UILayoutComponent ReadUILayoutComponent(const YAML::Node &n)
{
    Keys(n,{"Kind","Direction","Main","Cross","Gap","LineGap","LineAlign","GridGap","Columns","Rows"});UILayoutComponent v;
    if(n["Kind"])v.Kind=GameUI::ContainerKind(n["Kind"].as<int>());
    if(n["Direction"])v.Direction=GameUI::Axis(n["Direction"].as<int>());
    if(n["Main"])v.Main=ReadAlign(n["Main"]);
    if(n["Cross"])v.Cross=ReadAlign(n["Cross"]);
    if(n["LineAlign"])v.LineAlign=ReadAlign(n["LineAlign"]);
    if(n["Gap"])v.Gap=n["Gap"].as<float>();
    if(n["LineGap"])v.LineGap=n["LineGap"].as<float>();
    if(n["GridGap"])v.GridGap=Vector<2>(n["GridGap"]);
    if(n["Columns"])v.Columns=ReadTracks(n["Columns"]);
    if(n["Rows"])v.Rows=ReadTracks(n["Rows"]);
    GameUI::Validate(v);return v;
}
static void Write(YAML::Emitter &out,const UILayoutComponent &v)
{
    out<<YAML::BeginMap;
    out<<YAML::Key<<"Kind"<<YAML::Value<<int(v.Kind);
    out<<YAML::Key<<"Direction"<<YAML::Value<<int(v.Direction);
    out<<YAML::Key<<"Main"<<YAML::Value<<int(v.Main);
    out<<YAML::Key<<"Cross"<<YAML::Value<<int(v.Cross);
    out<<YAML::Key<<"Gap"<<YAML::Value<<v.Gap;
    out<<YAML::Key<<"LineGap"<<YAML::Value<<v.LineGap;
    out<<YAML::Key<<"LineAlign"<<YAML::Value<<int(v.LineAlign);
    out<<YAML::Key<<"GridGap"<<YAML::Value;Write(out,v.GridGap);
    auto tracks=[&](const char*name,const auto &list){out<<YAML::Key<<name<<YAML::Value<<YAML::BeginSeq;for(const auto&t:list)out<<YAML::Flow<<YAML::BeginMap<<YAML::Key<<"Mode"<<YAML::Value<<int(t.Mode)<<YAML::Key<<"Value"<<YAML::Value<<t.Value<<YAML::Key<<"Minimum"<<YAML::Value<<t.Minimum<<YAML::Key<<"Maximum"<<YAML::Value<<t.Maximum<<YAML::Key<<"HasMaximum"<<YAML::Value<<t.HasMaximum<<YAML::EndMap;out<<YAML::EndSeq;};
    tracks("Columns",v.Columns);tracks("Rows",v.Rows);out<<YAML::EndMap;
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
    if(node["UINineSliceComponent"] && node["UIRectComponent"] && ReadUIRectComponent(node["UIRectComponent"]).Fit!=GameUI::ImageFit::Stretch)
        throw std::invalid_argument("Nine-slice owns border fitting and requires Image Fit Stretch");
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
