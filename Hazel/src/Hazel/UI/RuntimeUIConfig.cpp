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
    Keys(n, {"ReferenceSize", "SafeInsets", "SortOrder", "Visible", "Enabled", "Modal", "Screen"});
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
    if(auto nscreen=n["Screen"]){
        Keys(nscreen,{"Managed","InitiallyOpen","Cancel","InitialFocus","DefaultAction","CancelTarget","ReturnFocus","EventOwner"});
        auto &c=v.Screen;
        if(nscreen["Managed"])c.Managed=nscreen["Managed"].as<bool>();
        if(nscreen["InitiallyOpen"])c.InitiallyOpen=nscreen["InitiallyOpen"].as<bool>();
        if(nscreen["Cancel"]){auto value=nscreen["Cancel"].as<unsigned>();if(value>2)throw std::runtime_error("Invalid UI screen Cancel policy");c.Cancel=GameUI::ScreenCancel(value);}
        if(nscreen["InitialFocus"])c.InitialFocus=nscreen["InitialFocus"].as<uint64_t>();
        if(nscreen["DefaultAction"])c.DefaultAction=nscreen["DefaultAction"].as<uint64_t>();
        if(nscreen["CancelTarget"])c.CancelTarget=nscreen["CancelTarget"].as<uint64_t>();
        if(nscreen["ReturnFocus"])c.ReturnFocus=nscreen["ReturnFocus"].as<uint64_t>();
        if(nscreen["EventOwner"])c.EventOwner=nscreen["EventOwner"].as<uint64_t>();
    }
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
    const auto &c=v.Screen;
    out << YAML::Key << "Screen" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Managed" << YAML::Value << c.Managed;
    out << YAML::Key << "InitiallyOpen" << YAML::Value << c.InitiallyOpen;
    out << YAML::Key << "Cancel" << YAML::Value << unsigned(c.Cancel);
    out << YAML::Key << "InitialFocus" << YAML::Value << c.InitialFocus;
    out << YAML::Key << "DefaultAction" << YAML::Value << c.DefaultAction;
    out << YAML::Key << "CancelTarget" << YAML::Value << c.CancelTarget;
    out << YAML::Key << "ReturnFocus" << YAML::Value << c.ReturnFocus;
    out << YAML::Key << "EventOwner" << YAML::Value << c.EventOwner;
    out << YAML::EndMap << YAML::EndMap;
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
    Keys(n, {"FontPath", "Size", "Horizontal", "Vertical", "Wrap", "Overflow", "MaxLines"});
    UITextStyleComponent v;
    // An empty historical marker has no interpretation. Nonempty raw paths are
    // unsupported; FontSelection on Text is the only authored selection.
    if(n["FontPath"] && !n["FontPath"].as<std::string>().empty())
        throw std::runtime_error("Raw UI FontPath is unsupported; author Text.FontFamily (.hfont)");
    if (n["Size"])
        v.Size = n["Size"].as<float>();
    if (n["Horizontal"])
        v.Horizontal = ReadAlign(n["Horizontal"]);
    if (n["Vertical"])
        v.Vertical = ReadAlign(n["Vertical"]);
    if (n["Wrap"])
        v.Wrap = n["Wrap"].as<bool>();
    if(n["Overflow"]) {
        auto mode=n["Overflow"].as<std::string>();
        if(mode!="Clip" && mode!="Ellipsis")throw std::runtime_error("Text Overflow must be Clip or Ellipsis");
        v.Overflow=mode=="Clip"?Text::Overflow::Clip:Text::Overflow::Ellipsis;
    }
    if(n["MaxLines"])v.MaxLines=n["MaxLines"].as<uint32_t>();
    if(v.MaxLines>4096)throw std::runtime_error("Text MaxLines must be 0 (unlimited) to 4096");
    if(v.Horizontal>GameUI::Align::End||v.Vertical>GameUI::Align::End)throw std::runtime_error("Text alignment must be Start, Center or End");
    Positive(v.Size, false);
    if (v.Size > 512)
        throw std::runtime_error("UI font size exceeds 512");
    return v;
}
static void Write(YAML::Emitter &out, const UITextStyleComponent &v)
{
    out << YAML::BeginMap;
    out << YAML::Key << "Size" << YAML::Value;
    Write(out, v.Size);
    out << YAML::Key << "Horizontal" << YAML::Value;
    Write(out, v.Horizontal);
    out << YAML::Key << "Vertical" << YAML::Value;
    Write(out, v.Vertical);
    out << YAML::Key << "Wrap" << YAML::Value;
    Write(out, v.Wrap);
    out << YAML::Key << "Overflow" << YAML::Value << (v.Overflow==Text::Overflow::Clip?"Clip":"Ellipsis");
    out << YAML::Key << "MaxLines" << YAML::Value << v.MaxLines;
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
    Keys(n, {"Action", "TabOrder", "Interactable", "Colors", "Images", "Kind", "InitiallyChecked", "Indicator", "Group", "EventOwner"});
    UIButtonComponent v;
    if(n["Kind"]){auto kind=n["Kind"].as<unsigned>();if(kind>3)throw std::invalid_argument("Invalid UI control kind");v.Kind=GameUI::ControlKind(kind);}
    if(n["InitiallyChecked"])v.InitiallyChecked=n["InitiallyChecked"].as<bool>();
    if(n["Indicator"])v.Indicator=n["Indicator"].as<uint64_t>();
    if(n["Group"])v.Group=n["Group"].as<uint64_t>();
    if(n["EventOwner"])v.EventOwner=n["EventOwner"].as<uint64_t>();
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
    out << YAML::Key << "Kind" << YAML::Value << unsigned(v.Kind);
    out << YAML::Key << "InitiallyChecked" << YAML::Value << v.InitiallyChecked;
    out << YAML::Key << "Indicator" << YAML::Value << v.Indicator;
    out << YAML::Key << "Group" << YAML::Value << v.Group;
    out << YAML::Key << "EventOwner" << YAML::Value << v.EventOwner;
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
UIRadioGroupComponent ReadUIRadioGroupComponent(const YAML::Node &n){
    Keys(n,{"Required","InitialSelection","EventOwner","Action"});UIRadioGroupComponent v;
    if(n["Required"])v.Required=n["Required"].as<bool>();
    if(n["InitialSelection"])v.InitialSelection=n["InitialSelection"].as<uint64_t>();
    if(n["EventOwner"])v.EventOwner=n["EventOwner"].as<uint64_t>();
    if(n["Action"])v.Action=n["Action"].as<std::string>();
    if(v.Action.size()>128||v.Action.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid radio action");
    return v;
}
static void Write(YAML::Emitter &out,const UIRadioGroupComponent &v){
    out<<YAML::BeginMap<<YAML::Key<<"Required"<<YAML::Value<<v.Required
       <<YAML::Key<<"InitialSelection"<<YAML::Value<<v.InitialSelection
       <<YAML::Key<<"EventOwner"<<YAML::Value<<v.EventOwner
       <<YAML::Key<<"Action"<<YAML::Value<<v.Action<<YAML::EndMap;
}
UIProgressComponent ReadUIProgressComponent(const YAML::Node &n){
    Keys(n,{"Minimum","Maximum","InitialValue","Fill","Direction"});UIProgressComponent v;
    if(n["Minimum"])v.Minimum=n["Minimum"].as<float>();
    if(n["Maximum"])v.Maximum=n["Maximum"].as<float>();
    if(n["InitialValue"])v.InitialValue=n["InitialValue"].as<float>();
    if(n["Fill"])v.Fill=n["Fill"].as<uint64_t>();
    if(n["Direction"]){auto d=n["Direction"].as<unsigned>();if(d>3)throw std::invalid_argument("Invalid progress direction");v.Direction=GameUI::FillDirection(d);}
    if(!std::isfinite(v.Minimum)||!std::isfinite(v.Maximum)||!std::isfinite(v.InitialValue)||v.Minimum>=v.Maximum||!std::isfinite(v.Maximum-v.Minimum))throw std::invalid_argument("Progress needs a finite increasing range and value");
    return v;
}
static void Write(YAML::Emitter &out,const UIProgressComponent &v){
    out<<YAML::BeginMap<<YAML::Key<<"Minimum"<<YAML::Value<<v.Minimum
       <<YAML::Key<<"Maximum"<<YAML::Value<<v.Maximum
       <<YAML::Key<<"InitialValue"<<YAML::Value<<v.InitialValue
       <<YAML::Key<<"Fill"<<YAML::Value<<v.Fill
       <<YAML::Key<<"Direction"<<YAML::Value<<unsigned(v.Direction)<<YAML::EndMap;
}
UISliderComponent ReadUISliderComponent(const YAML::Node &n){
    Keys(n,{"Minimum","Maximum","Step","InitialValue","Track","Fill","Thumb","Direction"});UISliderComponent v;
    if(n["Minimum"])v.Minimum=n["Minimum"].as<float>();
    if(n["Maximum"])v.Maximum=n["Maximum"].as<float>();
    if(n["Step"])v.Step=n["Step"].as<float>();
    if(n["InitialValue"])v.InitialValue=n["InitialValue"].as<float>();
    if(n["Track"])v.Track=n["Track"].as<uint64_t>();
    if(n["Fill"])v.Fill=n["Fill"].as<uint64_t>();
    if(n["Thumb"])v.Thumb=n["Thumb"].as<uint64_t>();
    if(n["Direction"])v.Direction=GameUI::FillDirection(n["Direction"].as<unsigned>());
    v.Validate();if(!std::isfinite(v.InitialValue)||unsigned(v.Direction)>3)throw std::invalid_argument("Invalid slider value/direction");return v;
}
static void Write(YAML::Emitter &out,const UISliderComponent &v){
    out<<YAML::BeginMap<<YAML::Key<<"Minimum"<<YAML::Value<<v.Minimum<<YAML::Key<<"Maximum"<<YAML::Value<<v.Maximum
       <<YAML::Key<<"Step"<<YAML::Value<<v.Step<<YAML::Key<<"InitialValue"<<YAML::Value<<v.InitialValue
       <<YAML::Key<<"Track"<<YAML::Value<<v.Track<<YAML::Key<<"Fill"<<YAML::Value<<v.Fill<<YAML::Key<<"Thumb"<<YAML::Value<<v.Thumb
       <<YAML::Key<<"Direction"<<YAML::Value<<unsigned(v.Direction)<<YAML::EndMap;
}
UIDropdownComponent ReadUIDropdownComponent(const YAML::Node &n){
    Keys(n,{"Trigger","Popup","MinimumWidth","MaximumHeight","Gap","Placeholder"});UIDropdownComponent v;
    if(n["Trigger"])v.Trigger=n["Trigger"].as<uint64_t>();if(n["Popup"])v.Popup=n["Popup"].as<uint64_t>();
    if(n["MinimumWidth"])v.MinimumWidth=n["MinimumWidth"].as<float>();if(n["MaximumHeight"])v.MaximumHeight=n["MaximumHeight"].as<float>();
    if(n["Gap"])v.Gap=n["Gap"].as<float>();if(n["Placeholder"])v.Placeholder=n["Placeholder"].as<std::string>();GameUI::Validate(v);return v;
}
static void Write(YAML::Emitter &out,const UIDropdownComponent &v){
    out<<YAML::BeginMap<<YAML::Key<<"Trigger"<<YAML::Value<<v.Trigger<<YAML::Key<<"Popup"<<YAML::Value<<v.Popup
       <<YAML::Key<<"MinimumWidth"<<YAML::Value<<v.MinimumWidth<<YAML::Key<<"MaximumHeight"<<YAML::Value<<v.MaximumHeight
       <<YAML::Key<<"Gap"<<YAML::Value<<v.Gap<<YAML::Key<<"Placeholder"<<YAML::Value<<v.Placeholder<<YAML::EndMap;
}
UIScrollViewComponent ReadUIScrollViewComponent(const YAML::Node &n){
    Keys(n,{"Content","Horizontal","Vertical","HorizontalTrack","HorizontalThumb","VerticalTrack","VerticalThumb","PixelsPerUnit","MinimumThumb","RevealMargin"});UIScrollViewComponent v;
#define READ(field,type) if(n[#field])v.field=n[#field].as<type>()
    READ(Content,uint64_t);READ(Horizontal,bool);READ(Vertical,bool);READ(HorizontalTrack,uint64_t);READ(HorizontalThumb,uint64_t);READ(VerticalTrack,uint64_t);READ(VerticalThumb,uint64_t);READ(PixelsPerUnit,float);READ(MinimumThumb,float);READ(RevealMargin,float);
#undef READ
    GameUI::Validate(v);return v;
}
static void Write(YAML::Emitter &out,const UIScrollViewComponent &v){
    out<<YAML::BeginMap;
#define WRITE(field) out<<YAML::Key<<#field<<YAML::Value<<v.field
    WRITE(Content);WRITE(Horizontal);WRITE(Vertical);WRITE(HorizontalTrack);WRITE(HorizontalThumb);WRITE(VerticalTrack);WRITE(VerticalThumb);WRITE(PixelsPerUnit);WRITE(MinimumThumb);WRITE(RevealMargin);
#undef WRITE
    out<<YAML::EndMap;
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
    if (auto n = node["UIButtonComponent"]) ReadUIButtonComponent(n);
    if (auto n = node["UIRadioGroupComponent"]) ReadUIRadioGroupComponent(n);
    if (auto n = node["UIProgressComponent"]) ReadUIProgressComponent(n);
    if(auto n=node["UISliderComponent"])ReadUISliderComponent(n);
    if(auto n=node["UIScrollViewComponent"])ReadUIScrollViewComponent(n);
    if(auto n=node["UIDropdownComponent"])ReadUIDropdownComponent(n);
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
    if (auto n = node["UIButtonComponent"]) entity.AddComponent<UIButtonComponent>(ReadUIButtonComponent(n));
    if (auto n = node["UIRadioGroupComponent"]) entity.AddComponent<UIRadioGroupComponent>(ReadUIRadioGroupComponent(n));
    if (auto n = node["UIProgressComponent"]) entity.AddComponent<UIProgressComponent>(ReadUIProgressComponent(n));
    if(auto n=node["UISliderComponent"])entity.AddComponent<UISliderComponent>(ReadUISliderComponent(n));
    if(auto n=node["UIScrollViewComponent"])entity.AddComponent<UIScrollViewComponent>(ReadUIScrollViewComponent(n));
    if(auto n=node["UIDropdownComponent"])entity.AddComponent<UIDropdownComponent>(ReadUIDropdownComponent(n));
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
    if(entity.HasComponent<UIRadioGroupComponent>()){out<<YAML::Key<<"UIRadioGroupComponent"<<YAML::Value;Write(out,entity.GetComponent<UIRadioGroupComponent>());}
    if(entity.HasComponent<UIDropdownComponent>()){out<<YAML::Key<<"UIDropdownComponent"<<YAML::Value;Write(out,entity.GetComponent<UIDropdownComponent>());}
    if(entity.HasComponent<UIScrollViewComponent>()){out<<YAML::Key<<"UIScrollViewComponent"<<YAML::Value;Write(out,entity.GetComponent<UIScrollViewComponent>());}
    if(entity.HasComponent<UISliderComponent>()){out<<YAML::Key<<"UISliderComponent"<<YAML::Value;Write(out,entity.GetComponent<UISliderComponent>());}
    if(entity.HasComponent<UIProgressComponent>()){out<<YAML::Key<<"UIProgressComponent"<<YAML::Value;Write(out,entity.GetComponent<UIProgressComponent>());}

}
std::string UIAuthoredState(Entity entity)
{
    if (!entity.HasComponent<UICanvasComponent>() && !entity.HasComponent<UIRectComponent>() &&
        !entity.HasComponent<UILayoutComponent>() && !entity.HasComponent<UITextStyleComponent>() &&
        !entity.HasComponent<UINineSliceComponent>() && !entity.HasComponent<UIButtonComponent>() && !entity.HasComponent<UIRadioGroupComponent>() && !entity.HasComponent<UIProgressComponent>() && !entity.HasComponent<UISliderComponent>() && !entity.HasComponent<UIScrollViewComponent>() && !entity.HasComponent<UIDropdownComponent>())
        return {};
    YAML::Emitter out;
    out.SetFloatPrecision(std::numeric_limits<float>::max_digits10);
    out << YAML::BeginMap;
    WriteUIComponents(out, entity);
    out << YAML::EndMap;
    return out.c_str();
}
} // namespace Hazel
