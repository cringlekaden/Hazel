#include "hzpch.h"
#include "MeasuredLayout.h"
#include <cmath>
#include <stdexcept>
namespace Hazel::GameUI
{
namespace
{
constexpr size_t None = std::numeric_limits<size_t>::max();
SizeMode Mode(const RectLayout &r, int axis) { return axis ? r.HeightMode : r.WidthMode; }
float Padding(const RectLayout &r, int axis) { return r.Padding[axis] + r.Padding[axis + 2]; }
float Margin(const RectLayout &r, int axis) { return r.Margin[axis] + r.Margin[axis + 2]; }
float Clamp(float value, const RectLayout &r, int axis)
{
    if (!std::isfinite(value) || value < 0)
        throw std::invalid_argument("UI measured dimension overflow");
    value = std::max(value, r.MinimumSize[axis]);
    return r.HasMaximum[axis] ? std::min(value, r.MaximumSize[axis]) : value;
}
int Main(const MeasureNode &n) { return n.Flow.Direction == Axis::Horizontal ? 0 : 1; }
} // namespace
void MeasuredLayout::Error(size_t index, int axis, const std::string &message) const
{
    std::string path;
    for (auto [n, a] : m_Path)
        path += (*m_Nodes)[n].Name + "#" + std::to_string((*m_Nodes)[n].ID) +
                (a ? ".height -> " : ".width -> ");
    const auto &node = (*m_Nodes)[index];
    throw std::invalid_argument("UI dependency: " + path + node.Name + "#" +
                                std::to_string(node.ID) + (axis ? ".height: " : ".width: ") +
                                message);
}
float MeasuredLayout::Available(size_t i, int axis)
{
    const auto &n = (*m_Nodes)[i];
    if(n.Unbounded[axis])Error(i,axis,"Scroll content has indefinite available space; use Fixed/Content, not Fill");
    if(n.Parent!=None && (*m_Nodes)[n.Parent].Container && (*m_Nodes)[n.Parent].Flow.Kind==ContainerKind::Grid) {
        GridTracks(n.Parent,axis);
        return std::max(0.f,m_SlotSizes[i][axis]-Margin(n.Layout,axis));
    }
    if(n.Parent!=None && (*m_Nodes)[n.Parent].Container && (*m_Nodes)[n.Parent].Flow.Kind==ContainerKind::Wrap){
        WrapLines(n.Parent);return std::max(0.f,m_SlotSizes[i][axis]-Margin(n.Layout,axis));
    }
    float parent =
        n.Parent == None
            ? m_External.Size[axis]
            : std::max(0.f, Size(n.Parent, axis) - Padding((*m_Nodes)[n.Parent].Layout, axis));
    return std::max(0.f, parent - n.Layout.Margin[axis] - n.Layout.Margin[axis + 2]);
}
float MeasuredLayout::Size(size_t i, int axis)
{
    if (m_State[i][axis] == 2)
        return m_Sizes[i][axis];
    if (m_State[i][axis] == 1)
        Error(i, axis, "Content/Fill or anchored Content dependency is indefinite");
    const auto &n = (*m_Nodes)[i];
    const auto &r = n.Layout;
    if (!n.Visible)
    {
        m_State[i][axis] = 2;
        return m_Sizes[i][axis] = 0;
    }
    m_State[i][axis] = 1;
    m_Path.emplace_back(i, axis);
    float desired = 0;
    switch (Mode(r, axis))
    {
    case SizeMode::Fixed:
        desired = r.FixedSize[axis];
        break;
    case SizeMode::Fill:
        if (n.Parent != None && (*m_Nodes)[n.Parent].Container &&
            (*m_Nodes)[n.Parent].Flow.Kind!=ContainerKind::Grid && Main((*m_Nodes)[n.Parent]) == axis)
        {
            if((*m_Nodes)[n.Parent].Flow.Kind==ContainerKind::Wrap)WrapLines(n.Parent);else Distribute(n.Parent);
            desired = m_Sizes[i][axis];
        }
        else
            desired = Available(i, axis);
        break;
    case SizeMode::Content:
    {
        float visualWidth = axis ? std::max(0.f, Size(i, 0) - Padding(r, 0)) : -1;
        auto visual = m_Intrinsic(i, visualWidth);
        if (!std::isfinite(visual[axis]) || visual[axis] < 0)
            Error(i, axis, "invalid intrinsic measurement");
        float children = 0;
        size_t visible = 0;
        if(n.Container && n.Flow.Kind==ContainerKind::Grid) {
            const auto &tracks=axis?n.Flow.Rows:n.Flow.Columns;
            for(const auto &t:tracks) {
                if(t.Mode==TrackMode::Fill)Error(i,axis,"Fill tracks need a definite container axis");
                children+=t.HasMaximum?std::min(std::max(t.Value,t.Minimum),t.Maximum):std::max(t.Value,t.Minimum);
            }
            children+=(tracks.size()-1)*n.Flow.GridGap[axis];
        }
        else if(n.Container && n.Flow.Kind==ContainerKind::Wrap){
            if(Main(n)==axis)Error(i,axis,"Wrap requires Fixed/Fill main size; Content main is indefinite");
            WrapLines(i);const auto &lines=m_Groups[i].Lines;
            for(const auto &line:lines)children+=line.Thickness;
            if(!lines.empty())children+=(lines.size()-1)*n.Flow.LineGap;
        }
        else for (auto child : n.Children)
        {
            const auto &c = (*m_Nodes)[child];
            if (!c.Visible)
                continue;
            if (!n.Container && (c.Layout.Anchor[axis] != 0 || c.Layout.Pivot[axis] != 0))
                Error(child, axis,
                      "a Content parent requires zero child anchor/pivot on this axis");
            float extent = Size(child, axis) + Margin(c.Layout, axis);
            if (n.Container && Main(n) == axis)
            {
                children += extent;
                ++visible;
            }
            else
                children = std::max(children, extent + (n.Container ? 0 : c.Layout.Offset[axis]));
        }
        if (n.Container && n.Flow.Kind==ContainerKind::Stack && Main(n) == axis && visible)
            children += (visible - 1) * n.Flow.Gap;
        desired = std::max(visual[axis], children) + Padding(r, axis);
        break;
    }
    }
    if (!std::isfinite(desired) || desired < 0)
        Error(i, axis, "measured dimension overflow");
    m_Results[i].Measured[axis] = desired;
    m_Sizes[i][axis] = Clamp(desired, r, axis);
    m_State[i][axis] = 2;
    m_Path.pop_back();
    return m_Sizes[i][axis];
}
void MeasuredLayout::Distribute(size_t parent)
{
    if (m_Distribution[parent] == 2)
        return;
    const auto &p = (*m_Nodes)[parent];
    const int axis = Main(p);
    if (m_Distribution[parent] == 1)
        Error(parent, axis, "recursive stack allocation");
    m_Distribution[parent] = 1;
    float available = std::max(0.f, Size(parent, axis) - Padding(p.Layout, axis));
    float used = 0;
    size_t count = 0;
    for (auto i : p.Children)
    {
        const auto &n = (*m_Nodes)[i];
        if (!n.Visible)
            continue;
        ++count;
        float size =
            Mode(n.Layout, axis) == SizeMode::Fill ? n.Layout.MinimumSize[axis] : Size(i, axis);
        if (Mode(n.Layout, axis) == SizeMode::Fill)
            m_Sizes[i][axis] = size;
        used += size + n.Layout.Margin[axis] + n.Layout.Margin[axis + 2];
    }
    if (count)
        used += (count - 1) * p.Flow.Gap;
    if (!std::isfinite(used))
        Error(parent, axis, "stack extent overflow");
    m_Results[parent].Overflow |= used > available;
    m_Allocations.clear();
    for(auto i:p.Children) {
        const auto &n=(*m_Nodes)[i];
        if(n.Visible && Mode(n.Layout,axis)==SizeMode::Fill)
            m_Allocations.push_back({i,n.Layout.MinimumSize[axis],n.Layout.MaximumSize[axis],n.Weight,0,n.Layout.HasMaximum[axis]});
    }
    m_Spare[parent]=Allocate(std::max(0.f,available-used));
    for(auto &a:m_Allocations) {
        m_Sizes[a.Index][axis]=a.Result;m_State[a.Index][axis]=2;
        m_Results[a.Index].Measured[axis]=a.Result;
    }
    m_Distribution[parent] = 2;
}
float MeasuredLayout::Allocate(float free)
{
    double uncapped=0,remaining=free,level=0;
    m_Caps.clear();
    for(size_t i=0;i<m_Allocations.size();++i) {
        const auto &a=m_Allocations[i];
        if(a.Capped)m_Caps.emplace_back((double(a.Maximum)-a.Minimum)/a.Weight,i);
        else uncapped+=a.Weight;
    }
    std::sort(m_Caps.begin(),m_Caps.end());
    m_CapWeights.assign(m_Caps.size()+1,uncapped);
    for(size_t k=m_Caps.size();k>0;--k)m_CapWeights[k-1]=m_CapWeights[k]+m_Allocations[m_Caps[k-1].second].Weight;
    for(size_t k=0;k<m_Caps.size();++k) {
        double active=m_CapWeights[k];if(active<=0)break;
        double cost=(m_Caps[k].first-level)*active;
        if(remaining<=cost){level+=remaining/active;remaining=0;break;}
        remaining-=cost;level=m_Caps[k].first;
    }
    if(uncapped>0){level+=remaining/uncapped;remaining=0;}
    for(auto &a:m_Allocations) {
        double result=double(a.Minimum)+double(a.Weight)*level;
        if(a.Capped)result=std::min(result,double(a.Maximum));
        if(!std::isfinite(result)||result>std::numeric_limits<float>::max())throw std::invalid_argument("Fill allocation overflow");
        a.Result=float(result);
    }
    return float(remaining);
}
void MeasuredLayout::PlaceGrid(size_t parent)
{
    auto &group=m_Groups[parent];if(group.Placed)return;
    const auto &n=(*m_Nodes)[parent];
    const int rows=int(n.Flow.Rows.size()),columns=int(n.Flow.Columns.size());
    std::array<uint64_t,64> occupied{};
    size_t work=0;
    for(auto child:n.Children)Validate((*m_Nodes)[child].Layout);
    auto reserve=[&](size_t child,int row,int col,bool test){
        const auto &r=(*m_Nodes)[child].Layout;
        if(row<0||col<0||row>=rows||col>=columns||row+int(r.RowSpan)>rows||col+int(r.ColumnSpan)>columns)return false;
        uint64_t mask=r.ColumnSpan==64?~uint64_t(0):((uint64_t(1)<<r.ColumnSpan)-1)<<col;
        for(int y=row;y<row+int(r.RowSpan);++y){
            if(++work>16*1024*1024)Error(child,0,"grid placement work limit exceeded; use explicit cells");
            if(occupied[y]&mask)return false;
        }
        if(test)return true;
        for(int y=row;y<row+int(r.RowSpan);++y)occupied[y]|=mask;
        m_Results[child].Row=row;m_Results[child].Column=col;return true;
    };
    for(auto child:n.Children) {
        const auto &r=(*m_Nodes)[child].Layout;
        if(r.GridRow>=0 && !reserve(child,r.GridRow,r.GridColumn,false))
            Error(child,0,"grid cell/span is out of range or occupied");
    }
    for(auto child:n.Children) {
        if((*m_Nodes)[child].Layout.GridRow>=0)continue;
        bool placed=false;
        const auto &placement=(*m_Nodes)[child].Layout;
        if(placement.RowSpan==1&&placement.ColumnSpan==1){
            const uint64_t legal=columns==64?~uint64_t(0):(uint64_t(1)<<columns)-1;
            for(int row=0;row<rows;++row){
                if(++work>16*1024*1024)Error(child,0,"grid placement work limit exceeded; use explicit cells");
                uint64_t free=~occupied[row]&legal;if(!free)continue;
                int col=0;while(!(free&(uint64_t(1)<<col)))++col;
                placed=reserve(child,row,col,false);break;
            }
        }else for(int cell=0;cell<rows*columns;++cell)
            if(reserve(child,cell/columns,cell%columns,true)) {
                reserve(child,cell/columns,cell%columns,false);placed=true;break;
            }
        if(!placed)Error(child,0,"grid has no free cell for this span; add tracks or change placement");
    }
    group.Placed=true;
}
void MeasuredLayout::GridTracks(size_t parent,int axis)
{
    auto &g=m_Groups[parent];if(g.GridState[axis]==2)return;
    if(g.GridState[axis]==1)Error(parent,axis,"recursive grid track allocation");
    g.GridState[axis]=1;PlaceGrid(parent);
    const auto &n=(*m_Nodes)[parent];const auto &tracks=axis?n.Flow.Rows:n.Flow.Columns;
    // Determine the container before taking shared scratch storage: dependencies
    // may resolve other containers recursively.
    float available=std::max(0.f,Size(parent,axis)-Padding(n.Layout,axis));
    auto &sizes=g.Tracks[axis],&starts=g.Starts[axis];sizes.resize(tracks.size());starts.resize(tracks.size());
    float used=(tracks.size()-1)*n.Flow.GridGap[axis];
    m_Allocations.clear();
    for(size_t t=0;t<tracks.size();++t) {
        const auto &v=tracks[t];
        sizes[t]=v.Mode==TrackMode::Fill?v.Minimum:std::max(v.Minimum,v.Value);
        if(v.HasMaximum)sizes[t]=std::min(sizes[t],v.Maximum);
        used+=sizes[t];
        if(v.Mode==TrackMode::Fill)m_Allocations.push_back({t,v.Minimum,v.Maximum,v.Value,0,v.HasMaximum});
    }
    if(!std::isfinite(used))Error(parent,axis,"grid track extent overflow");
    m_Results[parent].Overflow|=used>available;
    Allocate(std::max(0.f,available-used));
    for(auto &a:m_Allocations)sizes[a.Index]=a.Result;
    float cursor=0;
    for(size_t t=0;t<tracks.size();++t){starts[t]=cursor;cursor+=sizes[t]+n.Flow.GridGap[axis];}
    for(auto child:n.Children) {
        int first=axis?m_Results[child].Row:m_Results[child].Column;
        unsigned span=axis?(*m_Nodes)[child].Layout.RowSpan:(*m_Nodes)[child].Layout.ColumnSpan;
        m_SlotOffsets[child][axis]=starts[first];
        m_SlotSizes[child][axis]=starts[first+span-1]+sizes[first+span-1]-starts[first];
    }
    g.GridState[axis]=2;
}
void MeasuredLayout::WrapLines(size_t parent)
{
    auto &g=m_Groups[parent];if(g.WrapState==2)return;
    const auto &n=(*m_Nodes)[parent];int axis=Main(n),cross=1-axis;
    if(g.WrapState==1)Error(parent,cross,"indefinite wrap dependency: line sizing needs a child's definite main size");
    if(Mode(n.Layout,axis)==SizeMode::Content)Error(parent,axis,"Wrap requires Fixed/Fill main size; Content main is indefinite");
    g.WrapState=1;
    float available=std::max(0.f,Size(parent,axis)-Padding(n.Layout,axis));
    g.Items.clear();g.Lines.clear();
    for(auto child:n.Children)if((*m_Nodes)[child].Visible){
        const auto &c=(*m_Nodes)[child];
        float preferred=Mode(c.Layout,axis)==SizeMode::Fill?c.Layout.MinimumSize[axis]:Size(child,axis);
        m_Sizes[child][axis]=preferred;g.Items.push_back(child);
        float outer=preferred+Margin(c.Layout,axis);
        if(!std::isfinite(outer))Error(child,axis,"wrap item extent overflow");
        if(g.Lines.empty() || (g.Lines.back().End>g.Lines.back().Begin && g.Lines.back().Used+n.Flow.Gap+outer>available))
            g.Lines.push_back({g.Items.size()-1,g.Items.size()-1,0,0,0});
        auto &line=g.Lines.back();if(line.End>line.Begin)line.Used+=n.Flow.Gap;
        line.Used+=outer;++line.End;
    }
    for(size_t index=0;index<g.Lines.size();++index){
        auto &line=g.Lines[index];m_Allocations.clear();
        m_Results[parent].Overflow|=line.Used>available;
        for(size_t k=line.Begin;k<line.End;++k){auto i=g.Items[k];const auto &c=(*m_Nodes)[i];
            if(Mode(c.Layout,axis)==SizeMode::Fill)m_Allocations.push_back({i,c.Layout.MinimumSize[axis],c.Layout.MaximumSize[axis],c.Weight,0,c.Layout.HasMaximum[axis]});}
        line.Spare=Allocate(std::max(0.f,available-line.Used));
        for(auto &a:m_Allocations){m_Sizes[a.Index][axis]=a.Result;m_State[a.Index][axis]=2;m_Results[a.Index].Measured[axis]=a.Result;}
        for(size_t k=line.Begin;k<line.End;++k){auto i=g.Items[k];const auto &c=(*m_Nodes)[i];
            float extent=Mode(c.Layout,cross)==SizeMode::Fill?c.Layout.MinimumSize[cross]:Size(i,cross);
            line.Thickness=std::max(line.Thickness,extent+Margin(c.Layout,cross));}
        for(size_t k=line.Begin;k<line.End;++k){auto i=g.Items[k];const auto &c=(*m_Nodes)[i];
            m_Results[i].Line=int(index);m_SlotSizes[i][axis]=available;m_SlotSizes[i][cross]=line.Thickness;
            if(Mode(c.Layout,cross)==SizeMode::Fill){float desired=std::max(0.f,line.Thickness-Margin(c.Layout,cross));m_Sizes[i][cross]=Clamp(desired,c.Layout,cross);m_State[i][cross]=2;m_Results[i].Measured[cross]=desired;}}
        if(!std::isfinite(line.Thickness))Error(parent,cross,"wrap line extent overflow");
    }
    g.WrapState=2;
}
Rect MeasuredLayout::Content(size_t i)
{
    return m_Results[i].Bounds.Inset((*m_Nodes)[i].Layout.Padding);
}
void MeasuredLayout::Arrange(size_t i)
{
    const auto &n = (*m_Nodes)[i];
    if (!n.Visible)
        return;
    auto parent = n.Parent == None ? m_External : Content(n.Parent);
    auto &out = m_Results[i];
    out.Bounds.Size = m_Sizes[i];
    for (int axis = 0; axis < 2; ++axis)
    {
        out.Available[axis] = std::max(0.f, (n.Parent!=None && (*m_Nodes)[n.Parent].Container && (*m_Nodes)[n.Parent].Flow.Kind!=ContainerKind::Stack ? m_SlotSizes[i][axis] : parent.Size[axis]) - Margin(n.Layout, axis));
        if(n.Unbounded[axis])out.Available[axis]=-1;
        out.Bounds.Position[axis] = parent.Position[axis] +
                                    n.Layout.Anchor[axis] * parent.Size[axis] +
                                    n.Layout.Offset[axis] + n.Layout.Margin[axis] -
                                    n.Layout.Pivot[axis] * out.Bounds.Size[axis];
    }
    if (n.Parent != None && (*m_Nodes)[n.Parent].Container)
        out.Bounds.Position = m_Positions[i];
    for (int axis = 0; axis < 2; ++axis)
        if (!std::isfinite(out.Bounds.Position[axis]) ||
            !std::isfinite(out.Bounds.Position[axis] + out.Bounds.Size[axis]))
            Error(i, axis, "arranged position overflow");
    auto inside = out.Bounds.Intersect(parent);
    for(int axis=0;axis<2;++axis)out.Overflow |= !n.Unbounded[axis]&&(inside.Position[axis]!=out.Bounds.Position[axis]||inside.Size[axis]!=out.Bounds.Size[axis]);
    if(n.Container && n.Flow.Kind==ContainerKind::Grid) {
        GridTracks(i,0);GridTracks(i,1);auto content=Content(i);
        for(int axis=0;axis<2;++axis)for(size_t t=0;t<m_Groups[i].Tracks[axis].size();++t) {
            Rect track=content;track.Position[axis]+=m_Groups[i].Starts[axis][t];track.Size[axis]=m_Groups[i].Tracks[axis][t];
            m_Guides.push_back({i,axis,int(t),false,track});
        }
        for(auto child:n.Children) {
            auto &result=m_Results[child];result.Slot={content.Position+m_SlotOffsets[child],m_SlotSizes[child]};
            for(int axis=0;axis<2;++axis) {
                const auto &r=(*m_Nodes)[child].Layout;
                float space=std::max(0.f,result.Slot.Size[axis]-Margin(r,axis));
                auto align=axis?n.Flow.Cross:n.Flow.Main;
                m_Positions[child][axis]=result.Slot.Position[axis]+r.Margin[axis]+std::max(0.f,space-m_Sizes[child][axis])*(align==Align::Center?.5f:align==Align::End?1.f:0.f);
                result.Overflow|=m_Sizes[child][axis]>space;
            }
        }
    }
    else if(n.Container && n.Flow.Kind==ContainerKind::Wrap){
        WrapLines(i);int axis=Main(n),cross=1-axis;auto content=Content(i);auto &g=m_Groups[i];
        float extent=g.Lines.empty()?0:(g.Lines.size()-1)*n.Flow.LineGap;
        for(const auto &line:g.Lines)extent+=line.Thickness;
        if(!std::isfinite(extent))Error(i,cross,"wrap block extent overflow");
        m_Results[i].Overflow|=extent>content.Size[cross];
        float crossCursor=content.Position[cross]+std::max(0.f,content.Size[cross]-extent)*(n.Flow.LineAlign==Align::Center?.5f:n.Flow.LineAlign==Align::End?1.f:0.f);
        for(size_t l=0;l<g.Lines.size();++l){const auto &line=g.Lines[l];
            Rect band=content;band.Position[cross]=crossCursor;band.Size[cross]=line.Thickness;
            m_Guides.push_back({i,cross,int(l),true,band});
            float cursor=content.Position[axis]+line.Spare*(n.Flow.Main==Align::Center?.5f:n.Flow.Main==Align::End?1.f:0.f);
            for(size_t k=line.Begin;k<line.End;++k){auto child=g.Items[k];const auto &r=(*m_Nodes)[child].Layout;
                m_Results[child].Slot=band;
                m_Positions[child][axis]=cursor+r.Margin[axis];
                float spare=std::max(0.f,line.Thickness-Margin(r,cross)-m_Sizes[child][cross]);
                m_Positions[child][cross]=crossCursor+r.Margin[cross]+spare*(n.Flow.Cross==Align::Center?.5f:n.Flow.Cross==Align::End?1.f:0.f);
                cursor+=m_Sizes[child][axis]+Margin(r,axis)+n.Flow.Gap;
            }
            crossCursor+=line.Thickness+n.Flow.LineGap;
        }
    }
    else if (n.Container)
    {
        Distribute(i);
        int axis = Main(n), cross = 1 - axis;
        auto content = Content(i);
        float spare = m_Spare[i];
        float cursor = content.Position[axis] + spare * (n.Flow.Main == Align::Center ? .5f
                                                         : n.Flow.Main == Align::End  ? 1.f
                                                                                      : 0.f);
        for (auto c : n.Children)
        {
            const auto &child = (*m_Nodes)[c];
            if (!child.Visible)
                continue;
            m_Positions[c][axis] = cursor + child.Layout.Margin[axis];
            float crossAvailable = std::max(0.f, content.Size[cross] - child.Layout.Margin[cross] -
                                                     child.Layout.Margin[cross + 2]);
            float crossSpare = std::max(0.f, crossAvailable - m_Sizes[c][cross]);
            m_Positions[c][cross] = content.Position[cross] + child.Layout.Margin[cross] +
                                    crossSpare * (n.Flow.Cross == Align::Center ? .5f
                                                  : n.Flow.Cross == Align::End  ? 1.f
                                                                                : 0.f);
            cursor += m_Sizes[c][axis] + child.Layout.Margin[axis] + child.Layout.Margin[axis + 2] +
                      n.Flow.Gap;
        }
    }
    for (auto child : n.Children)
        Arrange(child);
}
void MeasuredLayout::Resolve(const std::vector<MeasureNode> &nodes, Rect external,
                             Intrinsic intrinsic, size_t activeCount)
{
    const size_t count = activeCount ? activeCount : nodes.size();
    m_Nodes = &nodes;
    m_Intrinsic = std::move(intrinsic);
    m_External = external;
    m_Path.clear();
    try
    {
        if (!count || count > nodes.size() || count > InputRouter::MaxButtons)
            throw std::invalid_argument("Invalid measured UI tree size");
        external.Inset({});
        m_Results.assign(count, {});
        m_State.assign(count, {});
        m_Sizes.assign(count, {});
        m_Positions.assign(count, {});
        m_Distribution.assign(count, 0);
        m_Spare.assign(count, 0);m_SlotSizes.assign(count,{});m_SlotOffsets.assign(count,{});m_Guides.clear();
        if(m_Groups.size()<count)m_Groups.resize(count);
        for(size_t i=0;i<count;++i){m_Groups[i].Placed=false;m_Groups[i].GridState={};m_Groups[i].WrapState=0;}
        m_Depths.assign(count,0);m_Incoming.assign(count,0);
        auto &depths=m_Depths,&incoming=m_Incoming;
        for (size_t i = 0; i < count; ++i)
        {
            const auto &n = nodes[i];
            Validate(n.Layout);
            if ((n.Parent != None && n.Parent >= i) ||
                !std::isfinite(n.Weight) || n.Weight <= 0)
                Error(i, 0, "invalid measured tree/weight");
            depths[i] = n.Parent == None ? 1 : depths[n.Parent] + 1;
            if (depths[i] > 256)
                Error(i, 0, "layout depth exceeds 256");
            if (n.Container)
            {
                Validate(n.Flow);
                if(n.Flow.Kind==ContainerKind::Wrap && Mode(n.Layout,Main(n))==SizeMode::Content)Error(i,Main(n),"Wrap requires a definite Fixed/Fill main size");
            }
            for (auto child : n.Children)
            {
                if (child <= i || child >= count || nodes[child].Parent != i)
                    Error(i, 0, "invalid child index");
                if (++incoming[child] != 1)
                    Error(child, 0, "duplicate layout child");
            }
            if (n.Parent == None || !nodes[n.Parent].Container)
                for (int axis = 0; axis < 2; ++axis)
                    if (Mode(n.Layout, axis) == SizeMode::Fill &&
                        (n.Layout.Anchor[axis] != 0 || n.Layout.Offset[axis] != 0 ||
                         n.Layout.Pivot[axis] != 0))
                        Error(i, axis,
                              "Fill owns free position; use margins, zero anchor/offset/pivot");
        }
        for (size_t i = 0; i < count; ++i)
            if (nodes[i].Parent != None && incoming[i] != 1)
                Error(i, 0, "missing layout child");
        for(size_t i=0;i<count;++i)if(nodes[i].Container && nodes[i].Flow.Kind==ContainerKind::Grid)PlaceGrid(i);
        for (size_t i = 0; i < count; ++i)
        {
            Size(i, 0);
            Size(i, 1);
        }
        for (size_t i = 0; i < count; ++i)
            if (nodes[i].Parent == None)
                Arrange(i);
        m_Intrinsic = {};
        m_Nodes = nullptr;
    }
    catch (...)
    {
        m_Intrinsic = {};
        m_Nodes = nullptr;
        throw;
    }
}
} // namespace Hazel::GameUI
