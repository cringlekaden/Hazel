#include "hzpch.h"
#include "RuntimeUIModel.h"
#include <cmath>
#include <stdexcept>
namespace Hazel::GameUI
{
namespace
{
template <glm::length_t N> bool Finite(const glm::vec<N, float> &v)
{
    for (int i = 0; i < N; i++)
        if (!std::isfinite(v[i]))
            return false;
    return true;
}
template <glm::length_t N> bool Nonnegative(const glm::vec<N, float> &v)
{
    if (!Finite(v))
        return false;
    for (int i = 0; i < N; i++)
        if (v[i] < 0)
            return false;
    return true;
}
void CheckRect(const Rect &r)
{
    if (!Finite(r.Position) || !Nonnegative(r.Size) || !Finite(r.Position + r.Size))
        throw std::invalid_argument("UI rectangle requires finite position and nonnegative size");
}
bool Usable(const Button &b)
{
    auto visible = b.Bounds.Intersect(b.Clip);
    return b.Visible && b.Interactable && b.Hit == HitPolicy::Control && visible.Size.x > 0 &&
           visible.Size.y > 0;
}
} // namespace
bool Rect::Contains(glm::vec2 p) const
{
    return Size.x > 0 && Size.y > 0 && p.x >= Position.x && p.y >= Position.y &&
           p.x < Position.x + Size.x && p.y < Position.y + Size.y;
}
Rect Rect::Inset(glm::vec4 b) const
{
    CheckRect(*this);
    if (!Nonnegative(b))
        throw std::invalid_argument("UI insets must be finite and nonnegative");
    return {Position + glm::vec2(b.x, b.y),
            glm::max(glm::vec2(0), Size - glm::vec2(b.x + b.z, b.y + b.w))};
}
Rect Rect::Intersect(const Rect &r) const
{
    CheckRect(*this);
    CheckRect(r);
    auto lo = glm::max(Position, r.Position), hi = glm::min(Position + Size, r.Position + r.Size);
    return {lo, glm::max(glm::vec2(0), hi - lo)};
}
glm::vec2 CanvasFit::ToLogical(glm::vec2 p) const
{
    return Scale > 0 ? p / Scale : glm::vec2(0);
}
CanvasFit FitCanvas(glm::vec2 pixels, const CanvasLayout &config)
{
    if (!Nonnegative(pixels) || !Finite(config.ReferenceSize) || config.ReferenceSize.x <= 0 ||
        config.ReferenceSize.y <= 0 || !Nonnegative(config.SafeInsets))
        throw std::invalid_argument("Invalid UI canvas dimensions or safe insets");
    CanvasFit fit;
    if (pixels.x == 0 || pixels.y == 0)
        return fit;
    fit.Scale = std::min(pixels.x / config.ReferenceSize.x, pixels.y / config.ReferenceSize.y);
    if (!std::isfinite(fit.Scale) || fit.Scale <= 0)
        throw std::invalid_argument("UI canvas scale out of range");
    fit.Bounds.Size = pixels / fit.Scale;
    CheckRect(fit.Bounds);
    fit.Content = fit.Bounds.Inset(config.SafeInsets);
    return fit;
}
void Validate(const RectLayout &c)
{
    if (!Nonnegative(c.FixedSize) || !Nonnegative(c.MaximumSize) ||
        c.WidthMode<SizeMode::Fixed || c.WidthMode>SizeMode::Content ||
        c.HeightMode<SizeMode::Fixed || c.HeightMode>SizeMode::Content ||
        !Finite(c.Anchor) || !Finite(c.Offset) || !Finite(c.Pivot) ||
        !Nonnegative(c.MinimumSize) || !Nonnegative(c.Margin) || !Nonnegative(c.Padding))
        throw std::invalid_argument("Invalid UI layout value");
    for(int a=0;a<2;++a)
        if(c.Anchor[a]<0 || c.Anchor[a]>1 || c.Pivot[a]<0 || c.Pivot[a]>1 ||
           (c.HasMaximum[a] && c.MaximumSize[a]<c.MinimumSize[a]))
            throw std::invalid_argument("UI anchors/pivots must be normalized; maximum must be at least minimum");
    if(c.GridRow < -1 || c.GridColumn < -1 || ((c.GridRow==-1)!=(c.GridColumn==-1)) ||
       !c.RowSpan || !c.ColumnSpan || c.RowSpan>64 || c.ColumnSpan>64)
        throw std::invalid_argument("Grid placement needs both row/column or Auto, and spans 1..64");
}
void Validate(const Track &c)
{
    if(c.Mode<TrackMode::Fixed || c.Mode>TrackMode::Fill || !std::isfinite(c.Value) ||
       c.Value<0 || (c.Mode==TrackMode::Fill && c.Value==0) || !std::isfinite(c.Minimum) ||
       c.Minimum<0 || !std::isfinite(c.Maximum) || c.Maximum<0 || (c.HasMaximum && c.Maximum<c.Minimum))
        throw std::invalid_argument("Tracks require finite Fixed sizes or positive Fill weights and ordered min/max");
}
void Validate(const FlowLayout &c)
{
    if(c.Kind<ContainerKind::Stack || c.Kind>ContainerKind::Wrap ||
       c.Direction<Axis::Horizontal || c.Direction>Axis::Vertical ||
       c.Main<Align::Start || c.Main>Align::End || c.Cross<Align::Start || c.Cross>Align::End ||
       c.LineAlign<Align::Start || c.LineAlign>Align::End ||
       !std::isfinite(c.Gap) || c.Gap<0 || !std::isfinite(c.LineGap) || c.LineGap<0 || !Nonnegative(c.GridGap) ||
       c.Rows.empty() || c.Columns.empty() || c.Rows.size()>64 || c.Columns.size()>64)
        throw std::invalid_argument("Invalid UI container configuration (tracks 1..64 per axis)");
    for(const auto &t:c.Columns)Validate(t);
    for(const auto &t:c.Rows)Validate(t);
}
ImagePlacement FitImage(const Rect &box,glm::vec2 source,ImageFit fit,glm::vec2 alignment)
{
    CheckRect(box);
    if(!Nonnegative(source)||!Finite(alignment)||alignment.x<0||alignment.y<0||alignment.x>1||alignment.y>1||fit<ImageFit::Stretch||fit>ImageFit::Native)
        throw std::invalid_argument("Image fit needs a valid mode, finite source size and normalized alignment");
    ImagePlacement out;out.Visual={box.Position,{}};
    if(box.Size.x==0||box.Size.y==0){out.UV.Size={};return out;}
    if(fit==ImageFit::Stretch){out.Visual=box;return out;}
    if(source.x==0||source.y==0){out.UV.Size={};return out;}
    if(fit==ImageFit::Contain){
        double scale=std::min(double(box.Size.x)/source.x,double(box.Size.y)/source.y);
        out.Visual.Size={float(source.x*scale),float(source.y*scale)};
        out.Visual.Position=box.Position+glm::max(glm::vec2(0),box.Size-out.Visual.Size)*alignment;
    }else if(fit==ImageFit::Cover){
        double scale=std::max(double(box.Size.x)/source.x,double(box.Size.y)/source.y);
        out.Visual=box;out.UV.Size={float(box.Size.x/(source.x*scale)),float(box.Size.y/(source.y*scale))};
        out.UV.Position=(glm::vec2(1)-out.UV.Size)*alignment;
    }else{
        out.Visual.Size=glm::min(box.Size,source);
        out.Visual.Position=box.Position+glm::max(glm::vec2(0),box.Size-source)*alignment;
        out.UV.Size=out.Visual.Size/source;out.UV.Position=(glm::vec2(1)-out.UV.Size)*alignment;
    }
    CheckRect(out.Visual);return out;
}
Rect FramebufferCoverage(const Rect &pixels, glm::vec2 ratio, glm::vec2 framebuffer)
{
    CheckRect(pixels);
    if (!Finite(ratio) || ratio.x <= 0 || ratio.y <= 0 || !Nonnegative(framebuffer) ||
        framebuffer.x > 2147483000.f || framebuffer.y > 2147483000.f)
        throw std::invalid_argument("Invalid UI framebuffer mapping");
    Rect scaled{pixels.Position * ratio, pixels.Size * ratio};
    auto clipped = scaled.Intersect({{}, framebuffer});
    auto lo = glm::ceil(clipped.Position), hi = glm::floor(clipped.Position + clipped.Size);
    return {lo, glm::max(glm::vec2(0), hi - lo)};
}
RectLayout EditMeasuredRect(const Rect &parent, const Rect &start, const RectLayout &initial,
                            glm::vec2 delta, unsigned edges)
{
    Validate(initial);CheckRect(parent);CheckRect(start);
    if(!Finite(delta) || edges>15 || ((edges&1)&&(edges&4)) || ((edges&2)&&(edges&8)))
        throw std::invalid_argument("Invalid measured UI gesture");
    auto result=initial;
    for(int axis=0;axis<2;++axis)
    {
        auto mode=axis?initial.HeightMode:initial.WidthMode;
        unsigned leading=axis?2:1,trailing=axis?8:4;
        if(!edges)
        {
            if(mode==SizeMode::Fill)throw std::invalid_argument("Fill owns position; edit margins in Properties");
            result.Offset[axis]+=delta[axis];continue;
        }
        if(!(edges&(leading|trailing)))continue;
        if(mode!=SizeMode::Fixed)throw std::invalid_argument("Fill/Content owns this dimension; edit its sizing constraint in Properties");
        float desired=std::max(initial.MinimumSize[axis],start.Size[axis]+((edges&leading)?-delta[axis]:delta[axis]));
        if(initial.HasMaximum[axis])desired=std::min(desired,initial.MaximumSize[axis]);
        if(desired==start.Size[axis])continue;
        float position=start.Position[axis]+((edges&leading)?start.Size[axis]-desired:0);
        result.FixedSize[axis]=desired;
        result.Offset[axis]=position-parent.Position[axis]-initial.Anchor[axis]*parent.Size[axis]+
            initial.Pivot[axis]*desired-initial.Margin[axis];
    }
    Validate(result);return result;
}
namespace
{
constexpr size_t NoRank = static_cast<size_t>(-1);
uint64_t Hit(const std::vector<Button> &buttons, glm::vec2 point)
{
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
    {
        if (!it->Visible || it->Hit == HitPolicy::PassThrough ||
            !it->Bounds.Intersect(it->Clip).Contains(point))
            continue;
        return Usable(*it) ? it->ID : 0; // Block/disabled controls occlude lower controls.
    }
    return 0;
}
} // namespace
void InputRouter::Reset()
{
    m_Scene = m_Focus = m_Hover = m_Capture = m_KeyCapture = m_Generation = m_Dropped = 0;
    m_Ready = m_Left = m_Accept = m_Next = m_Previous = m_PointerGesture = false;
    m_Scope=m_NavigationMisses=0;m_ReturnFocus.clear();m_Cancels=0;m_ScopeChanged=m_Cancel=false;
    m_Order.clear();
    m_IDs.clear();
    m_Index.clear();
    m_Ranks.clear();
    m_Actions.clear();
}
void InputRouter::Reconcile(uint64_t scene, const std::vector<Button> &buttons, uint64_t generation)
{
    if (scene != m_Scene)
    {
        Reset();
        m_Scene = scene;
    }
    if (generation && generation == m_Generation)
        return;
    if (buttons.size() > MaxButtons)
        throw std::invalid_argument("UI exceeds 4096 hit entries");
    m_IDs.clear();
    for (auto &b : buttons)
    {
        CheckRect(b.Bounds);
        CheckRect(b.Clip);
        if (!b.ID || b.Action.size() > 128 || b.Action.find('\0') != std::string::npos ||
            (b.Hit != HitPolicy::PassThrough && b.Hit != HitPolicy::Control && b.Hit != HitPolicy::Block))
            throw std::invalid_argument("Invalid UI button identity/action");
        m_IDs.push_back(b.ID);
    }
    std::sort(m_IDs.begin(), m_IDs.end());
    if (std::adjacent_find(m_IDs.begin(), m_IDs.end()) != m_IDs.end())
        throw std::invalid_argument("Duplicate UI button identity");
    bool sameIDs = m_Index.size() == buttons.size();
    if (sameIDs)
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            auto found = m_Index.find(buttons[i].ID);
            if (found == m_Index.end() || found->second != i)
            {
                sameIDs = false;
                break;
            }
        }
    if (!sameIDs)
    {
        m_Index.clear();
        m_Index.reserve(buttons.size());
        for (size_t i = 0; i < buttons.size(); ++i)
            m_Index.emplace(buttons[i].ID, i);
    }
    m_Order.clear();
    m_Ranks.assign(buttons.size(), NoRank);
    for (size_t i = 0; i < buttons.size(); ++i)
        if (Usable(buttons[i]))
            m_Order.push_back(i);
    std::sort(m_Order.begin(), m_Order.end(), [&](size_t a, size_t b) {
        return buttons[a].TabOrder != buttons[b].TabOrder
                   ? buttons[a].TabOrder < buttons[b].TabOrder
                   : a < b;
    });
    for (size_t rank = 0; rank < m_Order.size(); ++rank)
        m_Ranks[m_Order[rank]] = rank;
    auto eligible = [&](uint64_t id) {
        auto found = m_Index.find(id);
        return found != m_Index.end() && m_Ranks[found->second] != NoRank;
    };
    if(m_ScopeChanged) {
        m_Focus=0;
        for(auto saved:m_ReturnFocus)if(saved.first==m_Scope&&eligible(saved.second))m_Focus=saved.second;
        m_ScopeChanged=false;
        if(!m_Focus&&!m_Order.empty())m_Focus=buttons[m_Order.front()].ID;
    }
    else if (!m_Ready)
        m_Focus = 0;
    else if (!eligible(m_Focus))
        m_Focus = m_Order.empty() ? 0 : buttons[m_Order.front()].ID;
    if (!eligible(m_Capture))
        m_Capture = 0;
    if (!eligible(m_KeyCapture))
        m_KeyCapture = 0;
    if (!eligible(m_Hover))
        m_Hover = 0;
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](const Action &a) {
                                       auto found = m_Index.find(a.Entity);
                                       return found == m_Index.end() ||
                                              !Usable(buttons[found->second]) ||
                                              buttons[found->second].Action != a.Name;
                                   }),
                    m_Actions.end());
    m_Generation = generation;
}
void InputRouter::SetScope(uint64_t scene,uint64_t scope)
{
    if(scene!=m_Scene){Reset();m_Scene=scene;}
    if(scope==m_Scope)return;
    auto saved=std::find_if(m_ReturnFocus.begin(),m_ReturnFocus.end(),[&](auto p){return p.first==m_Scope;});
    if(saved!=m_ReturnFocus.end())saved->second=m_Focus;
    else {if(m_ReturnFocus.size()==64)m_ReturnFocus.erase(m_ReturnFocus.begin());m_ReturnFocus.emplace_back(m_Scope,m_Focus);}
    m_Scope=scope;m_ScopeChanged=true;m_Generation=0;m_Capture=m_KeyCapture=0;m_Actions.clear();m_Cancels=0;
    // Retain physical gesture ownership until release: a newly opened scope cannot inherit a click.
}
bool InputRouter::ConsumeCancel(uint64_t scene,uint64_t scope)
{
    if(scene!=m_Scene||scope!=m_Scope||!m_Cancels)return false;
    --m_Cancels;return true;
}
void InputRouter::Navigate(const std::vector<Button>& buttons,glm::ivec2 direction)
{
    auto current=m_Index.find(m_Focus);if(current==m_Index.end()||m_Order.empty())return;
    const auto origin=buttons[current->second].Bounds.Intersect(buttons[current->second].Clip);
    const glm::vec2 center=origin.Position+origin.Size*.5f;
    size_t best=NoRank;double bestScore=std::numeric_limits<double>::infinity();
    for(auto index:m_Order){if(index==current->second)continue;
        auto rect=buttons[index].Bounds.Intersect(buttons[index].Clip);auto delta=rect.Position+rect.Size*.5f-center;
        double main=direction.x?delta.x*direction.x:delta.y*direction.y;
        if(main<=0)continue;
        double cross=direction.x?delta.y:delta.x;
        // Euclidean distance plus perpendicular penalty favors aligned nearby targets.
        double score=main*main+cross*cross*4.0;
        if(score<bestScore){bestScore=score;best=index;}
    }
    if(best!=NoRank){m_Focus=buttons[best].ID;m_KeyCapture=0;}else ++m_NavigationMisses;
}
void InputRouter::Update(uint64_t scene, const std::vector<Button> &buttons, const InputFrame &f,
                         uint64_t generation)
{
    if (!Finite(f.Mouse) || (f.PointerPressed && !Finite(f.PressPosition)) ||
        (f.PointerReleased && f.HasReleasePosition && !Finite(f.ReleasePosition)))
        throw std::invalid_argument("Invalid UI pointer frame");
    Reconcile(scene, buttons, generation);
    for(auto edge:f.Events)if(size_t(edge.Action)>=size_t(UIAction::Count)||!Finite(edge.Point))throw std::invalid_argument("Invalid UI action edge");
    if(f.Events.size()>64)throw std::invalid_argument("UI action edge limit exceeded (64)");
    if(f.Events.empty()){UpdateSample(scene,buttons,f);return;}
    // A pointer release owns this entire sampled gesture, even if Accept follows it.
    bool pointer=f.SuppressAccept||m_PointerGesture||m_Left;
    bool cancelling=f.CancelDown;
    for(auto e:f.Events)cancelling|=e.Action==UIAction::Cancel&&e.Down;
    for(auto e:f.Events)pointer|=e.Action==UIAction::Press;
    InputFrame step;step.Enabled=f.Enabled;step.KeyboardEnabled=f.KeyboardEnabled;
    step.Mouse=f.Mouse;step.LeftDown=m_Left;step.AcceptDown=m_Accept;step.SuppressAccept=pointer;
    const bool ready=m_Ready;
    if(!ready){step.LeftDown=f.LeftDown;step.AcceptDown=f.AcceptDown;UpdateSample(scene,buttons,step);return;}
    for(auto e:f.Events){
        step.PointerPressed=step.PointerReleased=step.AcceptPressed=false;step.CancelDown=cancelling;
        step.NextPressed=step.PreviousPressed=false;step.Navigate={};
        if(e.Action==UIAction::Press){step.Mouse=e.Point;step.LeftDown=e.Down;step.PointerPressed=e.Down;step.PointerReleased=!e.Down;step.PressPosition=step.ReleasePosition=e.Point;step.HasReleasePosition=true;}
        else if(e.Action==UIAction::Accept){step.AcceptDown=e.Down;step.AcceptPressed=e.Down;}
        else if(e.Action==UIAction::Cancel)step.CancelDown=cancelling;
        else if(e.Down){switch(e.Action){case UIAction::Next:step.NextPressed=true;break;case UIAction::Previous:step.PreviousPressed=true;break;case UIAction::Up:step.Navigate.y=-1;break;case UIAction::Down:step.Navigate.y=1;break;case UIAction::Left:step.Navigate.x=-1;break;case UIAction::Right:step.Navigate.x=1;break;default:break;}}
        UpdateSample(scene,buttons,step);
    }
    step.Mouse=f.Mouse;step.LeftDown=f.LeftDown;step.AcceptDown=f.AcceptDown;
    step.PointerPressed=step.PointerReleased=step.AcceptPressed=false;step.CancelDown=cancelling;step.NextPressed=step.PreviousPressed=false;step.Navigate={};
    UpdateSample(scene,buttons,step);
}
void InputRouter::UpdateSample(uint64_t scene,const std::vector<Button>& buttons,const InputFrame& f)
{
    const bool armed = m_Ready;
    m_Ready = scene != 0 && f.Enabled;
    m_Hover = 0;
    if (m_Ready)
    {
        m_Hover = Hit(buttons, f.Mouse);
        if (!m_Focus && !m_Order.empty())
            m_Focus = buttons[m_Order.front()].ID;
        if (armed && f.KeyboardEnabled && !m_Order.empty() &&
            (f.NextPressed || f.PreviousPressed || (f.NextDown && !m_Next) || (f.PreviousDown && !m_Previous)))
        {
            auto found = m_Index.find(m_Focus);
            size_t rank = found == m_Index.end() ? 0 : m_Ranks[found->second];
            const size_t count = m_Order.size();
            m_Focus =
                buttons[m_Order[(rank + ((f.PreviousPressed || (f.PreviousDown && !m_Previous)) ? count - 1 : 1)) % count]]
                    .ID;
        }
        if(armed&&f.KeyboardEnabled&&(f.Navigate.x||f.Navigate.y))Navigate(buttons,f.Navigate);
        const bool gestureOwns = f.SuppressAccept || m_PointerGesture || m_Capture || m_Left || f.LeftDown ||
                                 f.PointerPressed || f.PointerReleased;
        if (armed && (f.PointerPressed || (f.LeftDown && !m_Left)))
        {
            m_PointerGesture = true;
            m_Capture = Hit(buttons, f.PointerPressed ? f.PressPosition : f.Mouse);
            if (m_Capture)
                m_Focus = m_Capture;
        }
        if (f.CancelDown)
        {
            if(armed&&!m_Cancel&&f.KeyboardEnabled){if(m_Cancels<MaxActions)++m_Cancels;else ++m_Dropped;}
            m_Capture = m_KeyCapture = 0;
            m_Actions.clear();
        }
        if (m_KeyCapture != m_Focus || !f.AcceptDown || !f.KeyboardEnabled || gestureOwns)
            m_KeyCapture = 0;
        const bool accept = armed && f.KeyboardEnabled && !f.CancelDown && !gestureOwns &&
                            (f.AcceptPressed || (f.AcceptDown && !m_Accept));
        if (accept)
            m_KeyCapture = m_Focus;
        uint64_t activate = 0;
        if (armed && !f.CancelDown && (f.PointerReleased || (!f.LeftDown && m_Left)))
        {
            auto release = Hit(
                buttons, f.PointerReleased && f.HasReleasePosition ? f.ReleasePosition : f.Mouse);
            if (m_Capture == release)
                activate = m_Capture;
        }
        if (!f.LeftDown)
        {
            m_Capture = 0;
            m_PointerGesture = false;
        }
        if (!activate && accept)
            activate = m_Focus;
        if (activate)
        {
            auto found = m_Index.find(activate);
            if (found != m_Index.end())
            {
                const auto &b = buttons[found->second];
                if (Usable(b) && !b.Action.empty())
                {
                    if (m_Actions.size() < MaxActions)
                        m_Actions.push_back({b.ID, b.Action});
                    else
                        ++m_Dropped; // Explicit drop-newest; host reports the counter.
                }
            }
        }
    }
    else
    {
        m_Focus = m_Hover = m_Capture = m_KeyCapture = 0;
        m_PointerGesture = false;
        m_Actions.clear();m_Cancels=0;
    }
    m_Cancel=f.CancelDown;
    m_Left = f.LeftDown;
    m_Accept = f.AcceptDown;
    m_Next = f.NextDown;
    m_Previous = f.PreviousDown;
}
void InputRouter::Retire(uint64_t scene, uint64_t id)
{
    if (scene != m_Scene || !id)
        return;
    for(auto& saved:m_ReturnFocus)if(saved.second==id)saved.second=0;
    m_ReturnFocus.erase(std::remove_if(m_ReturnFocus.begin(),m_ReturnFocus.end(),[&](auto saved){return saved.first==id;}),m_ReturnFocus.end());
    if(m_Scope==id)m_Cancels=0;
    if (m_Focus == id)
        m_Focus = 0;
    if (m_Hover == id)
        m_Hover = 0;
    if (m_Capture == id)
        m_Capture = 0;
    if (m_KeyCapture == id)
        m_KeyCapture = 0;
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [id](const Action &a) { return a.Entity == id; }),
                    m_Actions.end());
}
bool InputRouter::Consume(uint64_t scene, Action &a)
{
    if (!scene || scene != m_Scene || m_Actions.empty())
        return false;
    a = std::move(m_Actions.front());
    m_Actions.pop_front();
    return true;
}
bool InputRouter::SetFocus(uint64_t scene, uint64_t id, const std::vector<Button> &buttons)
{
    auto found = m_Index.find(id);
    if (scene != m_Scene || !m_Ready || found == m_Index.end() || found->second >= buttons.size() ||
        buttons[found->second].ID != id || !Usable(buttons[found->second]))
        return false;
    m_Focus = id;
    m_KeyCapture = 0;
    return true;
}
ButtonState InputRouter::State(const Button &b) const
{
    if (!b.Interactable)
        return ButtonState::Disabled;
    if ((m_Capture == b.ID && m_Left && m_Hover == b.ID) || (m_KeyCapture == b.ID && m_Accept))
        return ButtonState::Pressed;
    if (m_Hover == b.ID)
        return ButtonState::Hover;
    if (m_Focus == b.ID)
        return ButtonState::Focused;
    return ButtonState::Normal;
}
} // namespace Hazel::GameUI
