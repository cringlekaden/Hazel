#include "hzpch.h"
#include "RuntimeUI.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel
{
using namespace GameUI;
namespace
{
struct DependencyBytes
{
    std::vector<uint8_t> &Bytes;
    template <class T> void Add(T value)
    {
        static_assert(std::is_arithmetic_v<T>);
        auto begin = reinterpret_cast<const uint8_t *>(&value);
        Bytes.insert(Bytes.end(), begin, begin + sizeof(value));
    }
    template <glm::length_t L, class T, glm::qualifier Q> void Add(const glm::vec<L, T, Q> &value)
    {
        for (int i = 0; i < L; ++i)
            Add(value[i]);
    }
    void Add(const std::string &value)
    {
        Add(value.size());
        Bytes.insert(Bytes.end(), value.begin(), value.end());
    }
    void Add(const std::filesystem::path &value) { Add(value.generic_u8string()); }
};
Rect Scaled(Rect r, glm::vec2 scale)
{
    return {r.Position * scale, r.Size * scale};
}
bool Nonempty(Rect r)
{
    return r.Size.x > 0 && r.Size.y > 0;
}
} // namespace
void RuntimeUI::CancelInput()
{
    EndTextField(false,true,false);m_TextPointerNeutral=m_TextRearm=false;
    if(m_TipVisible||m_TipCandidate){m_TipVisible=m_TipCandidate=0;m_TipElapsed=0;++m_DropdownRevision;}
    if(!m_ScopeCancellation){m_Choice.reset();m_ScreenRequests.erase(std::remove_if(m_ScreenRequests.begin(),m_ScreenRequests.end(),[](const auto &r){return r.Kind!=RequestKind::Screen;}),m_ScreenRequests.end());if(auto id=ActiveDropdown();id&&m_Scene.IsRunning()&&!m_Scene.IsStopping())RequestDropdown(id,false);}
    CancelScrollCapture(false);m_ScrollNeutral=false;
    EndSlider(false,true,false);
    if (m_InputEnabled || m_Input.Captured() || m_Input.Focused() || !m_Actions.empty())
    {
        m_OuterDrops += m_Input.DroppedActions();
        m_Input.Reset();
    }
    if(!m_RetainPageEvents){m_Diagnostics.RetiredEvents+=m_Actions.size();m_Actions.clear();}
    m_InputEnabled = false;
    m_ActionOwnsFrame=false;
    m_PointerOver = m_KeyboardOwns = false;
}
void RuntimeUI::Reset()
{
    EndTextField(false,true,false);m_TextStates.clear();m_TextTime=0;
    CancelInput();
    m_Resolved.Valid = false;
    m_Resolved.Modal = m_Resolved.Scope = 0;
    m_Resolved.Screens.clear();
    m_Resolved.Elements.clear();
    m_Resolved.Index.clear();
    m_Resolved.Buttons.clear();
    m_Resolved.PainterOrder.clear();
    m_Resolved.Guides.clear();
    m_Resolved.Scrolls.clear();m_Resolved.Dropdowns.clear();
    m_Resolved.Warnings.clear();
    m_Source.clear();
    m_ScrollState.clear();m_ScrollParts.clear();m_ScrollFocus=0;
    m_PageTabs.clear();m_TooltipBodies.clear();m_TabGroups.clear();m_TooltipIDs.clear();m_TabPreviews.clear();m_PageFocus=m_PageFocusPage=m_PageFallback=0;m_RetainPageEvents=false;m_TipCandidate=m_TipVisible=m_TipPreview=0;m_TipElapsed=0;
    m_DropdownIDs.clear();m_DropdownTriggers.clear();m_DropdownPopups.clear();m_Choice.reset();m_PreviewDropdown=m_DropdownFocus=0;++m_DropdownRevision;
    m_Cache.clear();m_Values.clear();m_ControlVisuals.clear();m_AssetOwner.reset();
    m_GPUTimer.reset();m_PaintSubmission=0;m_PreparedBytes=0;
    m_Screens.clear();m_OpenScreens.clear();m_ScreenRequests.clear();m_ScreenFocus.clear();
    m_LastScope=m_ReturnFallback=m_PendingFocus=m_PreviewScreen=0;m_TransitionNeutral=false;++m_ScreenRevision;
    m_Diagnostics = {};
    m_OuterDrops = m_ReportedDrops = 0;
    // Generation IDs never alias an older published snapshot, including Play/Stop.
}
void RuntimeUI::Retire(uint64_t id)
{
    if(m_TextEdit.ID==id||m_TextEdit.Terminal.Owner==id)EndTextField(false,true,false);m_TextStates.erase(id);
    if(id==m_TipCandidate||id==m_TipVisible||m_TooltipBodies.count(id)){m_TipCandidate=m_TipVisible=0;m_TipElapsed=0;++m_DropdownRevision;}
    if(id==m_TipPreview||m_TooltipBodies.count(id)){m_TipPreview=0;++m_DropdownRevision;}

    if(m_Bar.Owner&&(id==m_Bar.Owner||id==m_Bar.Track||id==m_Bar.Thumb||Descendant(id,m_Bar.Owner)))CancelScrollCapture(true);
    if(m_ScrollFocus==id)m_ScrollFocus=0;
    if(m_Slider.ID&&(m_Slider.ID==id||m_Slider.Terminal.Owner==id||m_Slider.Terminal.Screen==id||m_Values.at(m_Slider.ID).Track==id||m_Values.at(m_Slider.ID).Thumb==id||m_Values.at(m_Slider.ID).Fill==id))EndSlider(false,true);
    if(auto screen=Screen(m_Resolved.Scope);screen&&screen->Policy.EventOwner==id){RememberFocus(screen->ID,m_Input.Focused(),m_KeyboardOwns);CancelScreenInput();}

    if (m_Input.Focused() == id && !m_Resolved.Modal)
        m_KeyboardOwns = false;
    m_Input.Retire(m_Scene.GetIdentity(), id);
    if(m_PendingFocus==id)m_PendingFocus=0;
    for(auto &saved:m_ScreenFocus)if(saved.Target==id)saved.Target=0;
    m_ScreenFocus.erase(std::remove_if(m_ScreenFocus.begin(),m_ScreenFocus.end(),[&](auto saved){return saved.Scope==id;}),m_ScreenFocus.end());
    if(m_Choice&&(m_Choice->Event.Entity==id||m_Choice->Event.Origin==id||m_Choice->Event.Selection==id))m_Choice.reset();
    m_ScreenRequests.erase(std::remove_if(m_ScreenRequests.begin(),m_ScreenRequests.end(),[&](const ScreenRequest &r){
        if(r.ID==id||r.Option==id||r.Page==id)return true;
        auto root=m_Scene.GetEntityByUUID(r.ID);
        return r.Kind!=RequestKind::Screen&&root&&root.HasComponent<UIDropdownComponent>()&&(root.GetComponent<UIDropdownComponent>().Trigger==id||root.GetComponent<UIDropdownComponent>().Popup==id);
    }),m_ScreenRequests.end());
    if(auto active=ActiveDropdown();active&&m_Scene.IsRunning()&&!m_Scene.IsStopping()){
        auto root=m_Scene.GetEntityByUUID(active);if(root&&root.HasComponent<UIDropdownComponent>()){
            const auto &d=root.GetComponent<UIDropdownComponent>();if(d.Trigger==id||d.Popup==id)RequestDropdown(active,false);
        }
    }
    if(auto it=m_Cache.find(id);it!=m_Cache.end()){m_PreparedBytes-=it->second.Glyphs.capacity()*sizeof(Renderer2D::TextGlyph);m_Cache.erase(it);}
    auto pendingBefore=m_Actions.size();
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](const Action &a) { return a.Entity == id || a.Origin == id || a.Owner == id || a.Screen == id || (a.Page && (a.Selection==id||a.Page==id)); }),
                    m_Actions.end());
    m_Diagnostics.RetiredEvents+=pendingBefore-m_Actions.size();
}
void RuntimeUI::CaptureSources(SurfaceMetrics surface, bool previewOrphans)
{
    ++m_Diagnostics.SourceChecks;m_ScrollIDs.clear();m_DropdownIDs.clear();CaptureCompositions();
    auto assets=m_Scene.GetAssets();
    if(m_AssetOwner.owner_before(assets) || assets.owner_before(m_AssetOwner)) {
        // Ownership identity includes expired control blocks, so allocator address
        // reuse and equal numeric epochs cannot hide a project replacement.
        m_AssetOwner=assets;++m_AssetGeneration;m_Cache.clear();m_PreparedBytes=0;CancelInput();
    }
    m_Candidate.clear();
    m_RectIDs.clear();
    m_CanvasIDs.clear();
    DependencyBytes state{m_Candidate};
    state.Add(m_Scene.GetIdentity());state.Add(m_AssetGeneration);state.Add(m_DropdownRevision);state.Add(m_PreviewDropdown);state.Add(m_TipVisible);state.Add(m_TipPreview);for(auto group:m_TabGroups){state.Add(group);auto i=m_TabPreviews.find(group);state.Add(i==m_TabPreviews.end()?uint64_t(0):i->second);}
    state.Add(surface.Viewport);
    state.Add(surface.Framebuffer);
    state.Add(previewOrphans);state.Add(previewOrphans?m_PreviewScreen:uint64_t(0));
    // Fixed visuals are paint dependencies. Content additionally observes
    // text/font/base-image metrics; colors and button-state images remain paint.
    for (auto h : m_Scene.GetAllEntitiesWith<UIRectComponent>())
    {
        Entity e(h, &m_Scene);
        auto id = uint64_t(e.GetUUID());
        if (!m_Scene.IsEntityValid(id))
            continue;
        m_RectIDs.push_back(id);
        state.Add(id);
        auto relation = m_Scene.GetRelationship(e);
        state.Add(uint64_t(relation.Parent));
        state.Add(relation.Order);
        // Interrupted chains still publish canvas/inherited-state diagnostics.
        // Observe their ordinary ancestor links until another tracked UI node.
        auto ancestor = m_Scene.GetEntityByUUID(relation.Parent);
        size_t depth = 0;
        while (ancestor && !ancestor.HasComponent<UIRectComponent>() &&
               !ancestor.HasComponent<UICanvasComponent>())
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI interrupted ancestry exceeds depth 256");
            state.Add(uint64_t(ancestor.GetUUID()));
            auto link = m_Scene.GetRelationship(ancestor);
            state.Add(uint64_t(link.Parent));
            state.Add(link.Order);
            ancestor = m_Scene.GetEntityByUUID(link.Parent);
        }
        state.Add(uint64_t(0));
        auto &r = e.GetComponent<UIRectComponent>();
        {
            state.Add(int(r.WidthMode));state.Add(int(r.HeightMode));
            state.Add(r.FixedSize);state.Add(r.MaximumSize);
            state.Add(bool(r.HasMaximum.x));state.Add(bool(r.HasMaximum.y));
            if(r.WidthMode==SizeMode::Content || r.HeightMode==SizeMode::Content){
                auto assets=m_Scene.GetAssets();state.Add(assets?assets->Epoch():uint64_t(0));
                state.Add(e.HasComponent<TextComponent>());
                if(e.HasComponent<TextComponent>()){
                    auto &text=e.GetComponent<TextComponent>();
                    if(text.Font.Family.native().size()>1024 || unsigned(text.Font.Style)>3)throw std::invalid_argument("Invalid native text family/style");
                    if(text.TextString.size()>65536)throw std::invalid_argument("UI text exceeds 64 KiB");
                    state.Add(text.TextString);state.Add(text.Kerning);state.Add(text.LineSpacing);
                    auto style=e.HasComponent<UITextStyleComponent>()?e.GetComponent<UITextStyleComponent>():UITextStyleComponent{};
                    state.Add(text.Font.Family);state.Add(int(text.Font.Style));state.Add(style.Size);state.Add(style.Wrap);state.Add(int(style.Overflow));state.Add(style.MaxLines);
                }
                state.Add(e.HasComponent<SpriteRendererComponent>());
                if(e.HasComponent<SpriteRendererComponent>()){
                    auto &source=e.GetComponent<SpriteRendererComponent>().Source;state.Add(source.index());
                    if(auto t=std::get_if<TextureSpriteSource>(&source)){
                        state.Add(t->Texture);state.Add(uintptr_t(t->Resource.get()));
                        if(t->Resource){state.Add(t->Resource->GetWidth());state.Add(t->Resource->GetHeight());}
                    }
                    if(auto s=std::get_if<SpriteReference>(&source)){state.Add(s->Sheet);state.Add(s->Region);}
                }
            }
        }
        state.Add(r.Anchor);
        state.Add(r.Offset);
        state.Add(r.Pivot);
        state.Add(r.MinimumSize);
        state.Add(r.Margin);
        state.Add(r.Padding);
        state.Add(r.Visible);
        state.Add(r.Enabled);
        state.Add(r.ClipChildren);
        state.Add(r.Flex);
        state.Add(r.GridRow);state.Add(r.GridColumn);state.Add(r.RowSpan);state.Add(r.ColumnSpan);
        state.Add(e.HasComponent<UILayoutComponent>());
        if (e.HasComponent<UILayoutComponent>())
        {
            auto &f = e.GetComponent<UILayoutComponent>();
            Validate(f);
            state.Add(int(f.Direction));
            state.Add(int(f.Main));
            state.Add(int(f.Cross));
            state.Add(int(f.Kind));state.Add(f.Gap);state.Add(f.LineGap);state.Add(int(f.LineAlign));state.Add(f.GridGap);
            for(const auto *tracks:{&f.Columns,&f.Rows}) {
                state.Add(tracks->size());for(const auto&t:*tracks){state.Add(int(t.Mode));state.Add(t.Value);state.Add(t.Minimum);state.Add(t.Maximum);state.Add(t.HasMaximum);}
            }
        }
        state.Add(e.HasComponent<UIButtonComponent>());
        if (e.HasComponent<UIButtonComponent>())
        {
            auto &b = e.GetComponent<UIButtonComponent>();
            state.Add(b.Action);
            state.Add(b.TabOrder);
            state.Add(b.Interactable);if(b.Kind!=ControlKind::Button){state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UIButtonComponent)));}state.Add(int(b.Kind));state.Add(b.InitiallyChecked);state.Add(b.Indicator);state.Add(b.Group);state.Add(b.EventOwner);
            if(b.Kind!=ControlKind::Button&&b.Indicator)state.Add(Decoration(id,b.Indicator));
            if(unsigned(b.Kind)>4)throw std::invalid_argument("Invalid UI control kind");
        }
        state.Add(e.HasComponent<UIRadioGroupComponent>());
        if(e.HasComponent<UIRadioGroupComponent>()){auto &g=e.GetComponent<UIRadioGroupComponent>();state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UIRadioGroupComponent)));if(g.Action.size()>128||g.Action.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid radio action");state.Add(g.Required);state.Add(g.InitialSelection);state.Add(g.EventOwner);state.Add(g.Action);}
        state.Add(e.HasComponent<UIProgressComponent>());
        if(e.HasComponent<UIProgressComponent>()){auto &p=e.GetComponent<UIProgressComponent>();state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UIProgressComponent)));state.Add(p.Minimum);state.Add(p.Maximum);state.Add(p.InitialValue);state.Add(p.Fill);state.Add(int(p.Direction));if(p.Fill)state.Add(Decoration(id,p.Fill));}
        state.Add(e.HasComponent<UITextEntryComponent>());if(e.HasComponent<UITextEntryComponent>()){const auto &c=e.GetComponent<UITextEntryComponent>();state.Add(c.Placeholder);state.Add(c.MaxBytes);state.Add(c.MaxGraphemes);state.Add(c.MaxLines);state.Add(c.Multiline);state.Add(c.ReadOnly);state.Add(c.CaretColor);state.Add(c.SelectionColor);state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UITextEntryComponent)));}
        state.Add(e.HasComponent<UITabsComponent>());if(e.HasComponent<UITabsComponent>()){state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UITabsComponent)));state.Add(e.GetComponent<UITabsComponent>().RememberFocus);}
        state.Add(e.HasComponent<UITabComponent>());if(e.HasComponent<UITabComponent>()){auto &c=e.GetComponent<UITabComponent>();state.Add(c.Page);state.Add(c.InitialFocus);state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UITabComponent)));}
        state.Add(e.HasComponent<UITooltipComponent>());if(e.HasComponent<UITooltipComponent>()){auto &c=e.GetComponent<UITooltipComponent>();state.Add(c.Body);state.Add(c.Delay);state.Add(c.Gap);state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UITooltipComponent)));}
        state.Add(e.HasComponent<UIDropdownComponent>());
        if(e.HasComponent<UIDropdownComponent>()){
            const auto &d=e.GetComponent<UIDropdownComponent>();Validate(d);m_DropdownIDs.push_back(id);
            state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UIDropdownComponent)));
            state.Add(d.Trigger);state.Add(d.Popup);state.Add(d.MinimumWidth);state.Add(d.MaximumHeight);state.Add(d.Gap);state.Add(d.Placeholder);
            for(auto part:{d.Trigger,d.Popup}){state.Add(m_Scene.GetAuthoringLifetime(part));state.Add(m_Scene.GetAuthoringComponentVersion(part,typeid(UIRectComponent)));state.Add(m_Scene.GetAuthoringComponentVersion(part,typeid(UIButtonComponent)));state.Add(m_Scene.GetAuthoringComponentVersion(part,typeid(UIScrollViewComponent)));}
        }
        if(e.HasComponent<UIButtonComponent>()&&e.GetComponent<UIButtonComponent>().Kind==ControlKind::Radio){
            auto g=m_Scene.GetEntityByUUID(e.GetComponent<UIButtonComponent>().Group);
            if(g&&g.HasComponent<UIDropdownComponent>()){state.Add(e.HasComponent<TextComponent>());if(e.HasComponent<TextComponent>()){auto &text=e.GetComponent<TextComponent>().TextString;if(text.size()>65536)throw std::invalid_argument("Dropdown label exceeds 64 KiB");state.Add(text);}}
        }
        state.Add(e.HasComponent<UIScrollViewComponent>());
        if(e.HasComponent<UIScrollViewComponent>()){auto &v=e.GetComponent<UIScrollViewComponent>();Validate(v);m_ScrollIDs.push_back(id);
            state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UIScrollViewComponent)));
            state.Add(v.Content);state.Add(v.Horizontal);state.Add(v.Vertical);state.Add(v.HorizontalTrack);state.Add(v.HorizontalThumb);state.Add(v.VerticalTrack);state.Add(v.VerticalThumb);state.Add(v.PixelsPerUnit);state.Add(v.MinimumThumb);state.Add(v.RevealMargin);
            state.Add(m_Scene.GetAuthoringLifetime(v.Content));state.Add(m_Scene.GetAuthoringComponentVersion(v.Content,typeid(UIRectComponent)));
            for(auto part:{v.HorizontalTrack,v.HorizontalThumb,v.VerticalTrack,v.VerticalThumb})state.Add(Decoration(id,part));}
        state.Add(e.HasComponent<UISliderComponent>());
        if(e.HasComponent<UISliderComponent>()){auto &v=e.GetComponent<UISliderComponent>();state.Add(m_Scene.GetAuthoringLifetime(id));state.Add(m_Scene.GetAuthoringComponentVersion(id,typeid(UISliderComponent)));state.Add(v.Minimum);state.Add(v.Maximum);state.Add(v.Step);state.Add(v.InitialValue);state.Add(v.Track);state.Add(v.Fill);state.Add(v.Thumb);state.Add(unsigned(v.Direction));for(auto part:{v.Track,v.Fill,v.Thumb})state.Add(Decoration(id,part));}
        uint64_t valueOwner=e.HasComponent<UIRadioGroupComponent>()?e.GetComponent<UIRadioGroupComponent>().EventOwner:e.HasComponent<UIButtonComponent>()?e.GetComponent<UIButtonComponent>().EventOwner:0;
        if(valueOwner){auto owner=m_Scene.GetEntityByUUID(valueOwner);state.Add(m_Scene.GetAuthoringLifetime(valueOwner));state.Add(m_Scene.GetAuthoringComponentVersion(valueOwner,typeid(ScriptComponent)));
            if(owner&&owner.HasComponent<ScriptComponent>()){const auto &name=owner.GetComponent<ScriptComponent>().ClassName;if(name.size()>512||name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid value owner class");state.Add(name);}}
        state.Add(e.HasComponent<UICanvasComponent>());
    }
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UICanvasComponent>())
    {
        Entity e(h, &m_Scene);
        auto id = uint64_t(e.GetUUID());
        if (!m_Scene.IsEntityValid(id))
            continue;
        m_CanvasIDs.push_back(id);
        state.Add(id);
        auto &c = e.GetComponent<UICanvasComponent>();
        state.Add(c.ReferenceSize);
        state.Add(c.SafeInsets);
        state.Add(c.SortOrder);
        state.Add(c.Visible);
        state.Add(c.Enabled);
        state.Add(c.Modal);
        auto &screen=c.Screen;state.Add(screen.Managed);state.Add(screen.InitiallyOpen);state.Add(unsigned(screen.Cancel));
        state.Add(screen.InitialFocus);state.Add(screen.DefaultAction);state.Add(screen.CancelTarget);state.Add(screen.ReturnFocus);state.Add(screen.EventOwner);
        if(screen.Managed){
            auto owner=m_Scene.GetEntityByUUID(screen.EventOwner);state.Add(bool(owner)&&m_Scene.IsEntityValid(screen.EventOwner));
            if(owner&&m_Scene.IsEntityValid(screen.EventOwner)){state.Add(m_Scene.GetAuthoringLifetime(screen.EventOwner));state.Add(m_Scene.GetAuthoringComponentVersion(screen.EventOwner,typeid(ScriptComponent)));if(owner.HasComponent<ScriptComponent>()){const auto &name=owner.GetComponent<ScriptComponent>().ClassName;if(name.size()>512||name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid UI owner class (512 bytes, no NUL)");state.Add(name);}}
        }
        // Independent canvases may have ordinary world ancestors. Their parent
        // links are dependencies too, without scanning unrelated world entities.
        auto ancestor = e;
        size_t depth = 0;
        while (ancestor)
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI canvas ancestry exceeds depth 256");
            auto rel = m_Scene.GetRelationship(ancestor);
            state.Add(uint64_t(rel.Parent));
            state.Add(rel.Order);
            ancestor = m_Scene.GetEntityByUUID(rel.Parent);
            if (ancestor && !m_Scene.IsEntityValid(ancestor.GetUUID()))
                break;
        }
        state.Add(uint64_t(0));
    }
    m_DropdownTriggers.clear();m_DropdownPopups.clear();
    if(m_DropdownIDs.size()>32)throw std::invalid_argument("Dropdown declaration limit exceeded (32)");
    std::sort(m_DropdownIDs.begin(),m_DropdownIDs.end());
    for(auto id:m_DropdownIDs){auto &d=m_Scene.GetEntityByUUID(id).GetComponent<UIDropdownComponent>();auto trigger=m_Scene.GetEntityByUUID(d.Trigger),popup=m_Scene.GetEntityByUUID(d.Popup);if(trigger&&m_Scene.GetRelationship(trigger).Parent==UUID(id))m_DropdownTriggers.emplace(d.Trigger,id);if(popup&&m_Scene.GetRelationship(popup).Parent==UUID(id))m_DropdownPopups.emplace(d.Popup,id);}
    SyncScreens();SyncDropdownScreens();state.Add(m_ScreenRevision);
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UILayoutComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            state.Add(uint64_t(e.GetUUID()));
    }
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            state.Add(uint64_t(e.GetUUID()));
    }
    if (m_RectIDs.size() > InputRouter::MaxButtons)
        throw std::runtime_error("UI declared rect limit exceeded (4096, including hidden rects)");
}
void RuntimeUI::ResolveSurface(SurfaceMetrics surface, bool previewOrphans)
{
    m_Scene.CheckThread();
    const auto start = std::chrono::steady_clock::now();
    try
    {
        CaptureSources(surface, previewOrphans);
        const auto sourceEnd=std::chrono::steady_clock::now();
        m_Diagnostics.LastSourceMilliseconds=std::chrono::duration<double,std::milli>(sourceEnd-start).count();
        m_Diagnostics.LastLayoutMilliseconds=0;
        if (m_Resolved.Valid && m_Candidate == m_Source)
        {
            ++m_Diagnostics.Reuses;
            return;
        }
        SyncControls();SyncTextFields();PreparePages();
        Build(surface, previewOrphans);
        m_Diagnostics.LastLayoutMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-sourceEnd).count();
        m_Build.Generation = ++m_NextGeneration;
        m_Build.Valid = true;
        std::swap(m_Resolved, m_Build);
        m_Source.swap(m_Candidate);
        PublishScope();
        SyncSliders();ReconcileTextField();
        ReconcileScrollCapture();
        PruneActions();
        FocusDropdownOption();FinishDropdownChoice();FocusPage();
        if (!m_Input.Focused() && !m_Resolved.Modal)
            m_KeyboardOwns = false;
        // Remove retired cache entries, including now-interrupted rect subtrees.
        for (auto it = m_Cache.begin(); it != m_Cache.end();)
        {
            auto n = m_Resolved.Find(it->first);
            if (!n || !n->LayoutResolved){
                m_PreparedBytes-=it->second.Glyphs.capacity()*sizeof(Renderer2D::TextGlyph);
                it = m_Cache.erase(it);
            }
            else
                ++it;
        }
        ++m_Diagnostics.Resolves;
        m_Diagnostics.LastResolveMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
    }
    catch (...)
    {
        m_RetainPageEvents=false;
        m_PageFocus=m_PageFocusPage=m_PageFallback=0;
        CancelInput();
        m_Resolved.Valid = false;
        throw;
    }
}
void RuntimeUI::Build(SurfaceMetrics surface, bool previewOrphans)
{
    // FitCanvas supplies finite/nonnegative logical validation, even at zero size.
    FitCanvas(surface.Viewport, {});
    if (!std::isfinite(surface.Framebuffer.x) || !std::isfinite(surface.Framebuffer.y) ||
        surface.Framebuffer.x < 0 || surface.Framebuffer.y < 0 ||
        surface.Framebuffer.x > 2147483000.f || surface.Framebuffer.y > 2147483000.f)
        throw std::invalid_argument("Invalid UI framebuffer size");
    m_Build.Elements.clear();
    m_Build.PainterOrder.clear();
    m_Build.Buttons.clear();
    m_Build.Warnings.clear();
    m_Build.Modal = m_Build.Scope = 0;
    m_Build.Screens.clear();
    m_Build.Guides.clear();m_Build.Scrolls.clear();m_Build.Dropdowns.clear();m_Build.Pages.clear();m_Build.Tooltips.clear();
    m_Build.Valid = false;
    m_Build.Surface = surface;
    m_Build.PreviewOrphans = previewOrphans;
    m_Build.SceneIdentity = m_Scene.GetIdentity();
    auto add = [&](uint64_t id) {
        auto previous = m_Build.Index.find(id);
        if (previous != m_Build.Index.end() && previous->second < m_Build.Elements.size() &&
            m_Build.Elements[previous->second].ID == id)
            return;
        auto e = m_Scene.GetEntityByUUID(id);
        ResolvedElement n;
        n.ID = id;
        n.Parent = m_Scene.GetRelationship(e).Parent;
        n.IsCanvas = e.HasComponent<UICanvasComponent>();
        n.Visible = n.Enabled = true;
        auto ancestor = e;
        size_t depth = 0;
        bool interrupted = false, foundCanvas = false;
        while (ancestor)
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI ancestry exceeds depth 256");
            if (ancestor.HasComponent<UIRectComponent>())
            {
                auto &r = ancestor.GetComponent<UIRectComponent>();
                Validate(r);
                if (!n.IsCanvas || ancestor != e)
                {
                    n.Visible &= r.Visible;
                    n.Enabled &= r.Enabled;
                }
            }
            if (ancestor.HasComponent<UICanvasComponent>())
            {
                auto &c = ancestor.GetComponent<UICanvasComponent>();
                FitCanvas(surface.Viewport, c);
                n.Visible &= c.Visible;
                n.Enabled &= c.Enabled;
                if (!foundCanvas)
                {
                    n.Canvas = ancestor.GetUUID();
                    foundCanvas = true;
                }
            }
            else if (ancestor != e && !foundCanvas && !ancestor.HasComponent<UIRectComponent>())
                interrupted = true;
            ancestor = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(ancestor).Parent);
        }
        if (!n.IsCanvas && interrupted)
            m_Build.Warnings.push_back(
                {id, ResolvedSnapshot::WarningCode::Interrupted});
        else if (!n.IsCanvas && !foundCanvas)
            m_Build.Warnings.push_back({id, ResolvedSnapshot::WarningCode::Orphan});
        if (n.IsCanvas && uint64_t(n.Parent))
            m_Build.Warnings.push_back(
                {id, ResolvedSnapshot::WarningCode::NestedCanvas});
        n.Scope=n.Canvas;
        n.Visible &= PageActive(id);
        for(auto parent=id;parent;){if(m_PageTabs.count(parent)){n.Page=parent;break;}auto p=m_Scene.GetEntityByUUID(parent);if(!p)break;parent=m_Scene.GetRelationship(p).Parent;}
        auto parent = m_Scene.GetEntityByUUID(n.Parent);
        n.ContainerOwned = !n.IsCanvas && parent && parent.HasComponent<UIRectComponent>() &&
                      !parent.HasComponent<UICanvasComponent>() && parent.HasComponent<UILayoutComponent>();
        if (e.HasComponent<UIRectComponent>())
        {
            auto &r = e.GetComponent<UIRectComponent>();
            n.WidthMode=r.WidthMode;n.HeightMode=r.HeightMode;
            if(n.IsCanvas)throw std::invalid_argument("Canvas owns its surface; place measured Rect on a child");
            if (!std::isfinite(r.Flex) || r.Flex < 0)
                throw std::invalid_argument("Invalid UI flex");
        }
        if (e.HasComponent<UIButtonComponent>())
        {
            auto &b = e.GetComponent<UIButtonComponent>();
            if (b.Action.size() > 128 || b.Action.find('\0') != std::string::npos)
                throw std::invalid_argument("Invalid UI action (128 bytes, no NUL)");
        }
        // Keep hash nodes on stable membership, including changed-layout frames.
        m_Build.Index.insert_or_assign(id, m_Build.Elements.size());
        m_Build.Elements.push_back(n);
    };
    for (auto id : m_RectIDs)
        add(id);
    for (auto id : m_CanvasIDs)
        add(id);
    for (auto it = m_Build.Index.begin(); it != m_Build.Index.end();)
        if (it->second >= m_Build.Elements.size() || m_Build.Elements[it->second].ID != it->first)
            it = m_Build.Index.erase(it);
        else
            ++it;
    // Diagnose component configurations that otherwise silently disappear.
    for (auto h : m_Scene.GetAllEntitiesWith<UILayoutComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            m_Build.Warnings.push_back({e.GetUUID(), ResolvedSnapshot::WarningCode::ContainerRect});
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            m_Build.Warnings.push_back({e.GetUUID(), ResolvedSnapshot::WarningCode::ButtonRect});
    }
    if (surface.Viewport.x == 0 || surface.Viewport.y == 0 || surface.Framebuffer.x == 0 ||
        surface.Framebuffer.y == 0)
        return;
    m_Canvases.clear();
    for (auto id : m_CanvasIDs)
        m_Canvases.push_back(m_Scene.GetEntityByUUID(id));
    std::sort(m_Canvases.begin(), m_Canvases.end(), [this](Entity a, Entity b) {
        auto x = a.GetComponent<UICanvasComponent>().SortOrder,
             y = b.GetComponent<UICanvasComponent>().SortOrder;
        auto &ca=a.GetComponent<UICanvasComponent>(),&cb=b.GetComponent<UICanvasComponent>();
        if(ca.Screen.Managed!=cb.Screen.Managed)return !ca.Screen.Managed;
        if(ca.Screen.Managed){auto ia=std::find(m_OpenScreens.begin(),m_OpenScreens.end(),uint64_t(a.GetUUID()));auto ib=std::find(m_OpenScreens.begin(),m_OpenScreens.end(),uint64_t(b.GetUUID()));if(ia!=ib)return ia<ib;}
        return x == y ? uint64_t(a.GetUUID()) < uint64_t(b.GetUUID()) : x < y;
    });
    for (auto canvas : m_Canvases)
    {
        auto &n = m_Build.Elements[m_Build.Index.at(canvas.GetUUID())];
        auto &c = canvas.GetComponent<UICanvasComponent>();
        if(c.Screen.Managed){
            bool open=IsScreenOpen(n.ID),available=n.Visible&&n.Enabled;
            m_Build.Screens.push_back({n.ID,c.Screen.EventOwner,open,available,c.Modal});
            if(previewOrphans){if(m_PreviewScreen? n.ID!=m_PreviewScreen:!open)continue;}
            else if(!open)continue;
            if(open&&available)m_Build.Scope=n.ID;
        }
        if (!n.Visible)continue;
        auto fit = FitCanvas(surface.Viewport, c);
        n.Scale = fit.Scale;
        n.Bounds = fit.Bounds;
        n.ParentContent = fit.Content;
        n.Clip = fit.Bounds;
        n.PixelBounds = Scaled(n.Bounds, glm::vec2(n.Scale));
        auto ratio = surface.Framebuffer / surface.Viewport;
        n.FramebufferBounds = Scaled(n.PixelBounds, ratio);
        n.FramebufferClip = FramebufferCoverage(n.PixelBounds, ratio, surface.Framebuffer);
        n.PixelClip = {n.FramebufferClip.Position / ratio, n.FramebufferClip.Size / ratio};
        n.LayoutResolved = true;
        if (c.Modal && n.Enabled)
            m_Build.Modal = n.ID;
        for (auto id : m_Scene.GetChildrenView(canvas.GetUUID()))
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e.HasComponent<UICanvasComponent>())
                Visit(e, fit.Content, fit.Bounds, fit.Scale, n.Enabled, canvas.GetUUID(), 0);
        }
    }
    if (previewOrphans)
    {
        auto fit = FitCanvas(surface.Viewport, {});
        for (auto id : m_RectIDs)
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e.HasComponent<UICanvasComponent>() &&
                !uint64_t(m_Scene.GetRelationship(e).Parent))
                Visit(e, fit.Content, fit.Bounds, fit.Scale, true, 0, 0);
        }
    }
    if(previewOrphans&&m_PreviewScreen){auto selected=m_Build.Find(m_PreviewScreen);if(selected&&selected->LayoutResolved)m_Build.Scope=m_PreviewScreen;}
    if(!m_Build.Scope)m_Build.Scope=m_Build.Modal;
    for(auto &n:m_Build.Elements){n.LayoutBounds=n.Bounds;n.LayoutParentContent=n.ParentContent;n.LayoutSlot=n.Slot;}
    for(auto &g:m_Build.Guides)g.LayoutBounds=g.Bounds;
    PrepareScrollState();ApplyScrollPresentation();ArrangeDropdown(true);ArrangeTooltip(true);BuildHitTargets();
    for(auto group:m_TabGroups)if(auto node=m_Build.Find(group);node&&node->LayoutResolved&&node->Visible&&PageActive(group)){auto selected=m_Scene.GetEntityByUUID(PageSelection(group));if(selected&&selected.HasComponent<UITabComponent>())m_Build.Pages.push_back({group,selected.GetUUID(),selected.GetComponent<UITabComponent>().Page});}
    for(const auto &entry:m_Screens){
        for(auto target:{entry.Policy.InitialFocus,entry.Policy.DefaultAction,entry.Policy.CancelTarget})if(target){
            auto n=m_Build.Find(target);auto e=m_Scene.GetEntityByUUID(target);
            if(!n||n->Scope!=entry.ID||!e||!e.HasComponent<UIButtonComponent>())
                m_Build.Warnings.push_back({entry.ID,ResolvedSnapshot::WarningCode::ScreenTarget});
        }
        if(entry.Policy.EventOwner&&!m_Scene.IsEntityValid(entry.Policy.EventOwner))m_Build.Warnings.push_back({entry.ID,ResolvedSnapshot::WarningCode::ScreenTarget});
    }
}
void RuntimeUI::BuildHitTargets(){
    m_Build.Buttons.clear();
    for (auto index : m_Build.PainterOrder)
    {
        auto &n = m_Build.Elements[index];
        auto e = m_Scene.GetEntityByUUID(n.ID);
        if (!e.HasComponent<UIButtonComponent>() || e.HasComponent<UIProgressComponent>() || e.HasComponent<UIRadioGroupComponent>())
            continue;
        auto &b = e.GetComponent<UIButtonComponent>();
        bool scope = n.Canvas && (!m_Build.Scope || n.Scope == m_Build.Scope);
        n.FocusEligible = scope && n.Enabled && b.Interactable && (b.Kind!=ControlKind::Radio||Member(b.Group,n.ID,false)) && Nonempty(n.PixelClip);
        n.Hit = !scope            ? HitPolicy::PassThrough
                : n.FocusEligible ? HitPolicy::Control
                                  : HitPolicy::Block;
        n.ButtonIndex = m_Build.Buttons.size();
        // Route in framebuffer space against the exact integer coverage used by
        // scissoring. Converting that region back to viewport pixels for hit
        // tests can exclude a covered edge through reciprocal rounding.
        m_Build.Buttons.push_back({n.ID, n.FramebufferBounds, n.FramebufferClip, b.Action,
                                  b.TabOrder, true, n.FocusEligible, n.Hit,b.Kind==ControlKind::Slider||b.Kind==ControlKind::TextEntry});
    }
    for(auto &v:m_Build.Scrolls){auto e=m_Scene.GetEntityByUUID(v.ID);auto &c=e.GetComponent<UIScrollViewComponent>();
        if(!v.Viewport.Size.x||!v.Viewport.Size.y)continue;
        for(int axis=0;axis<2;++axis){auto track=axis?c.VerticalTrack:c.HorizontalTrack;auto thumb=axis?c.VerticalThumb:c.HorizontalThumb;
            auto n=const_cast<ResolvedElement*>(m_Build.Find(track));
            if(!n||!thumb||!Decoration(v.ID,track)||!Decoration(v.ID,thumb)||v.Range[axis]<=0||!n->Visible||!n->Enabled||!n->LayoutResolved||(!m_Build.Scope?false:n->Scope!=m_Build.Scope))continue;
            n->ButtonIndex=m_Build.Buttons.size();n->Hit=HitPolicy::Control;
            auto ratio=m_Build.Surface.Framebuffer/m_Build.Surface.Viewport;auto rect=v.Tracks[axis];
            auto clip=FramebufferCoverage(Scaled(rect.Intersect(n->InheritedClip),glm::vec2(n->Scale)),ratio,m_Build.Surface.Framebuffer);
            m_Build.Buttons.push_back({track,Scaled(rect,glm::vec2(n->Scale)*ratio),clip,"",0,true,true,HitPolicy::Control,true,false});
        }
    }
    // Painter order includes internal pointer-only bars; their entity IDs are not focus targets.
    std::stable_sort(m_Build.Buttons.begin(),m_Build.Buttons.end(),[&](const auto&a,const auto&b){return m_Build.Find(a.ID)->Order<m_Build.Find(b.ID)->Order;});
    for(size_t i=0;i<m_Build.Buttons.size();++i)const_cast<ResolvedElement*>(m_Build.Find(m_Build.Buttons[i].ID))->ButtonIndex=i;
}
void RuntimeUI::Visit(Entity e, Rect parent, Rect clip, float scale, bool enabled, uint64_t canvas,
                      size_t depth, bool ownedRoot)
{
    if (!e || !m_Scene.IsEntityValid(e.GetUUID()) || !e.HasComponent<UIRectComponent>())
        return;
    if(OwnedPopup(e.GetUUID())&&!ownedRoot)return;
    if (depth >= Scene::MaxDepth)
        throw std::runtime_error("UI layout depth exceeded (256)");
    auto &r = e.GetComponent<UIRectComponent>();
    if (!r.Visible || !PageActive(e.GetUUID()))
        return;
    auto &n = m_Build.Elements[m_Build.Index.at(e.GetUUID())];
    if(!n.MeasureResolved)MeasureTree(e,parent);
    n.ParentContent = parent;
    n.Scale = scale;
    n.Enabled = enabled && r.Enabled;
    n.Visible = true;
    n.Canvas = canvas;n.Scope=canvas;
    n.LayoutResolved = true;
    n.Order = m_Build.PainterOrder.size();
    m_Build.PainterOrder.push_back(m_Build.Index.at(n.ID));
    n.InheritedClip=clip;
    n.Clip = clip.Intersect(n.Bounds);
    n.PixelBounds = Scaled(n.Bounds, glm::vec2(scale));
    auto ratio = m_Build.Surface.Framebuffer / m_Build.Surface.Viewport;
    n.FramebufferBounds = Scaled(n.PixelBounds, ratio);
    n.FramebufferClip =
        FramebufferCoverage(Scaled(n.Clip, glm::vec2(scale)), ratio, m_Build.Surface.Framebuffer);
    n.PixelClip = {n.FramebufferClip.Position / ratio, n.FramebufferClip.Size / ratio};
    auto content = n.Bounds.Inset(r.Padding),
         childClip = r.ClipChildren ? clip.Intersect(n.Bounds) : clip;
    auto inside = parent.Intersect(n.Bounds);
    bool outside=false;glm::bvec2 scrollAxis{false,false};auto p=m_Scene.GetEntityByUUID(n.Parent);
    if(p&&p.HasComponent<UIScrollViewComponent>()){const auto &v=p.GetComponent<UIScrollViewComponent>();if(v.Content==n.ID)scrollAxis={v.Horizontal,v.Vertical};}
    for(int axis=0;axis<2;++axis)outside|=!scrollAxis[axis]&&(inside.Position[axis]!=n.Bounds.Position[axis]||inside.Size[axis]!=n.Bounds.Size[axis]);
    n.Overflow |= outside;
    if (outside)
        m_Build.Warnings.push_back({n.ID, ResolvedSnapshot::WarningCode::Overflow});
    if(n.Overflow&&!outside)m_Build.Warnings.push_back({n.ID,ResolvedSnapshot::WarningCode::MeasureOverflow});
    const auto &children = m_Scene.GetChildrenView(e.GetUUID());
    for(auto id:children) {
        auto child=m_Scene.GetEntityByUUID(id);
        if(!child.HasComponent<UICanvasComponent>())
            Visit(child,content,childClip,scale,n.Enabled,canvas,depth+1);
    }
}
void RuntimeUI::PruneActions()
{
    auto before=m_Actions.size();
    m_Actions.erase(std::remove_if(m_Actions.begin(),m_Actions.end(),[&](const Action &a){return !EventValid(a);}),m_Actions.end());
    m_Diagnostics.RetiredEvents+=before-m_Actions.size();
}
bool RuntimeUI::Live(uint64_t id) const
{
    auto n = m_Resolved.Find(id);
    return m_Resolved.Valid && n && n->LayoutResolved && n->Visible && n->FocusEligible &&
           m_Scene.IsEntityValid(id);
}
void RuntimeUI::WarnOverflow()
{
    auto drops = m_OuterDrops + m_Input.DroppedActions();
    m_Diagnostics.DroppedActions = drops;
    auto now = std::chrono::steady_clock::now();
    if (drops != m_ReportedDrops &&
        (m_ReportedDrops == 0 || now - m_LastWarning >= std::chrono::seconds(1)))
    {
        HZ_CORE_WARN(
            "Runtime UI event queue full (64); newest event rejected (value changes not applied); total rejected {}",
            drops);
        m_ReportedDrops = drops;
        m_LastWarning = now;
    }
}
void RuntimeUI::UpdateInput(const InputFrame &pixels)
{
    if(!pixels.Enabled&&(m_TextEdit.ID||m_TextRearm||m_TextPointerNeutral))CancelInput();
    auto finite=[](glm::vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);};
    auto validPointer=[&](const InputFrame &f){return finite(f.Mouse)&&(!f.PointerPressed||finite(f.PressPosition))&&(!f.PointerReleased||!f.HasReleasePosition||finite(f.ReleasePosition));};
    if(!validPointer(pixels))throw std::invalid_argument("Invalid UI pointer frame");
    if(m_TransitionNeutral || m_SliderNeutral || m_ScrollNeutral){
        if(!pixels.Enabled){CancelInput();return;}
        const bool neutral=pixels.Enabled&&!pixels.LeftDown&&!pixels.AcceptDown&&!pixels.CancelDown&&!pixels.NextDown&&!pixels.PreviousDown&&!pixels.Navigate.x&&!pixels.Navigate.y&&!pixels.Adjust&&!pixels.Scroll.x&&!pixels.Scroll.y&&!pixels.PointerPressed&&!pixels.AcceptPressed&&!pixels.NextPressed&&!pixels.PreviousPressed&&
            std::none_of(pixels.Events.begin(),pixels.Events.end(),[](const ActionEdge &edge){return edge.Down;});
        if(!neutral){m_InputEnabled=false;return;}
        m_TransitionNeutral=false;m_SliderNeutral=false;m_ScrollNeutral=false;PublishScope();
        // Release-only samples establish neutral, but their releases never enter the new scope.
        auto ready=pixels;ready.Events.clear();ready.PointerReleased=false;ready.HasReleasePosition=false;
        UpdateInput(ready);return;
    }

    if (!m_Resolved.Valid)
        throw std::logic_error("UI input requires a resolved snapshot");
    const auto start = std::chrono::steady_clock::now();
    if(!pixels.Events.empty()) {
        if(pixels.Events.size()>64)throw std::invalid_argument("UI action edge limit exceeded (64)");
        InputFrame step;step.Enabled=pixels.Enabled;step.Mouse=pixels.Mouse;step.Shift=pixels.Shift;step.TextKeysHeld=pixels.TextKeysHeld;
        step.LeftDown=pixels.LeftDown;step.AcceptDown=pixels.AcceptDown;
        bool pointer=pixels.SuppressAccept||m_Input.Captured()||pixels.LeftDown;
        bool cancelling=pixels.CancelDown;
        bool firstPointer=true,firstAccept=true;size_t textBytes=0;
        for(auto edge:pixels.Events){
            textBytes+=edge.Text.Text.size();if(edge.Text.Text.size()>16384||textBytes>65536)throw std::invalid_argument("UI text edge payload limit exceeded");
            if(size_t(edge.Action)>=size_t(UIAction::Count)||!std::isfinite(edge.Point.x)||!std::isfinite(edge.Point.y)||!std::isfinite(edge.Delta.x)||!std::isfinite(edge.Delta.y))throw std::invalid_argument("Invalid UI action edge");
            if(edge.Action==UIAction::Scroll||(edge.Down&&edge.Action>=UIAction::ScrollUp&&edge.Action<=UIAction::ScrollRight))pointer=true;
            if(edge.Action==UIAction::Press){pointer=true;if(firstPointer){step.LeftDown=!edge.Down;firstPointer=false;}}
            if(edge.Action==UIAction::Accept&&firstAccept){step.AcceptDown=!edge.Down;firstAccept=false;}
            cancelling|=edge.Action==UIAction::Cancel&&edge.Down;
        }
        if(m_TextEdit.ID&&!pixels.AcceptDown&&!pixels.TextKeysHeld)m_TextEdit.SuppressText=false;
        bool owned=false;step.SuppressAccept=pointer;
        for(auto edge:pixels.Events){
            if(edge.Action==UIAction::Text){m_TextShift=edge.Text.Shift;TextCommand(edge.Text);owned|=TextEditing();continue;}
            step.PointerPressed=step.PointerReleased=step.AcceptPressed=false;
            step.NextDown=step.PreviousDown=step.NextPressed=step.PreviousPressed=step.FocusNavigation=false;
            step.Navigate={};step.Adjust=0;step.Scroll={};step.ScrollPage=false;step.CancelDown=cancelling;
            if(edge.Action==UIAction::Scroll){step.Mouse=edge.Point;step.Scroll=edge.Delta;}
            else if(edge.Action==UIAction::Press){step.Shift=edge.Device==InputDevice::Pointer?edge.Text.Shift:pixels.Shift;step.Mouse=edge.Point;step.LeftDown=edge.Down;step.PointerPressed=edge.Down;step.PointerReleased=!edge.Down;step.PressPosition=step.ReleasePosition=edge.Point;step.HasReleasePosition=true;}
            else if(edge.Action==UIAction::Accept){step.AcceptDown=edge.Down;step.AcceptPressed=edge.Down;}
            else if(edge.Down){switch(edge.Action){
                case UIAction::ScrollUp:step.Scroll={0,-1};step.ScrollPage=true;break;
                case UIAction::ScrollDown:step.Scroll={0,1};step.ScrollPage=true;break;
                case UIAction::ScrollLeft:step.Scroll={-1,0};step.ScrollPage=true;break;
                case UIAction::ScrollRight:step.Scroll={1,0};step.ScrollPage=true;break;
                case UIAction::AdjustDecrease:step.Adjust=-1;break;
                case UIAction::AdjustIncrease:step.Adjust=1;break;
                case UIAction::Next:step.FocusNavigation=step.NextPressed=true;break;
                case UIAction::Previous:step.FocusNavigation=step.PreviousDown=step.PreviousPressed=true;break;
                case UIAction::Up:step.FocusNavigation=true;step.Navigate.y=-1;break;
                case UIAction::Down:step.FocusNavigation=true;step.Navigate.y=1;break;
                case UIAction::Left:step.FocusNavigation=true;step.Navigate.x=-1;break;
                case UIAction::Right:step.FocusNavigation=true;step.Navigate.x=1;break;
                default:break;
            }}
            UpdateInput(step);owned|=m_ActionOwnsFrame;
        }
        step.Mouse=pixels.Mouse;step.LeftDown=pixels.LeftDown;step.AcceptDown=pixels.AcceptDown;step.CancelDown=pixels.CancelDown;
        step.PointerPressed=step.PointerReleased=step.AcceptPressed=false;
        step.NextDown=step.PreviousDown=step.NextPressed=step.PreviousPressed=step.FocusNavigation=false;step.Navigate={};step.Adjust=0;step.Scroll={};step.ScrollPage=false;
        UpdateInput(step);m_ActionOwnsFrame|=owned;
        m_Diagnostics.LastRoutingMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        return;
    }
    m_PointerOver = false;
    auto input = pixels;
    if(input.Mouse!=m_TipMouse||input.PointerPressed){m_TipPointer=true;m_TipMouse=input.Mouse;}
    if(input.FocusNavigation||input.AcceptPressed||input.Adjust||input.ScrollPage)m_TipPointer=false;
    const auto &surface = m_Resolved.Surface;
    const bool usableSurface = surface.Viewport.x > 0 && surface.Viewport.y > 0 &&
                               surface.Framebuffer.x > 0 && surface.Framebuffer.y > 0;
    input.Enabled &= usableSurface;
    if((!input.Enabled||input.PointerPressed||input.AcceptPressed||input.CancelDown)&&m_TipVisible){m_TipVisible=m_TipCandidate=0;m_TipElapsed=0;++m_DropdownRevision;}
    auto ratio = usableSurface ? surface.Framebuffer / surface.Viewport : glm::vec2(1);
    input.Mouse *= ratio;
    input.PressPosition *= ratio;
    input.ReleasePosition *= ratio;
    if(!validPointer(input))throw std::invalid_argument("UI pointer framebuffer conversion overflow");
    m_ActionOwnsFrame=input.Enabled&&(m_Input.Captured()||((m_KeyboardOwns||m_ActionOwnsFrame)&&input.CancelDown));
    m_TextShift=input.Shift;
    if(RouteTextFrame(input))return;
    const bool editing=m_Slider.ID!=0;
    input.Editing=editing;
    if(!input.Enabled){EndSlider(false,true);CancelScrollCapture(false);if(auto id=ActiveDropdown())RequestDropdown(id,false);}
    const bool previousKeyboardOwns=m_KeyboardOwns;
    m_InputEnabled = input.Enabled;
    uint64_t entryFocus = 0;
    int entryOrder =
        input.PreviousDown ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (auto &b : m_Resolved.Buttons)
    {
        if (b.Hit != HitPolicy::PassThrough && b.Bounds.Intersect(b.Clip).Contains(input.Mouse))
            m_PointerOver = true;
        if (!b.Interactable || !b.Focusable)
            continue;
        if (!entryFocus ||
            (input.PreviousDown ? b.TabOrder >= entryOrder : b.TabOrder < entryOrder))
        {
            entryFocus = b.ID;
            entryOrder = b.TabOrder;
        }
    }
    if(input.Enabled&&m_Bar.Owner&&input.CancelDown){CancelScrollCapture(true);m_ActionOwnsFrame=true;return;}
    if(input.Enabled&&RouteDropdown(input))return;
    if(m_Bar.Owner){input.Navigate={};input.NextDown=input.PreviousDown=input.NextPressed=input.PreviousPressed=input.FocusNavigation=false;}
    if(input.Enabled){auto hovered=HoverScroll(input.Mouse);m_PointerOver|=hovered!=0;
        if(input.PointerPressed&&!m_Slider.ID&&!m_Bar.Owner){m_ScrollFocus=hovered;if(hovered){m_KeyboardOwns=true;m_ActionOwnsFrame=true;}}
        m_ActionOwnsFrame|=RouteScroll(input);
        if(!editing)NavigateScroll(input);
    }
    if(input.Enabled&&m_Slider.ID&&!m_Slider.Pointer&&(input.PointerPressed||input.LeftDown)){
        const auto *target=m_Resolved.Find(m_Slider.ID);
        if(!target||!target->FramebufferBounds.Intersect(target->FramebufferClip).Contains(input.Mouse)){
            EndSlider(false,true);m_KeyboardOwns=false;m_ActionOwnsFrame=true;
            m_Diagnostics.InputGeneration=m_Resolved.Generation;
            m_Diagnostics.LastRoutingMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            return; // This press owns its release; it cannot become a gameplay click.
        }
    }
    bool entering = !m_Resolved.Modal && !m_KeyboardOwns && input.FocusNavigation && entryFocus &&
                    input.Enabled && !input.CancelDown;
    if (!input.Enabled)m_KeyboardOwns=false;
    else if(!m_Resolved.Modal) {
        if((input.CancelDown&&!editing) || (input.PointerPressed&&!m_PointerOver))m_KeyboardOwns=false;
        else if(input.FocusNavigation&&entryFocus)m_KeyboardOwns=true;
    }
    m_Input.SetScope(m_Scene.GetIdentity(),m_Resolved.Scope);
    input.KeyboardEnabled = !entering && (m_Resolved.Modal || m_KeyboardOwns || (previousKeyboardOwns&&input.CancelDown));
    if(m_PendingFocus&&m_Input.SetFocus(m_Scene.GetIdentity(),m_PendingFocus,m_Resolved.Buttons))m_PendingFocus=0;
    if(input.KeyboardEnabled&&!m_Input.Focused())if(auto screen=Screen(m_Resolved.Scope);screen&&Live(screen->Policy.DefaultAction))m_Input.SetFocus(m_Scene.GetIdentity(),screen->Policy.DefaultAction,m_Resolved.Buttons);
    m_Input.Update(m_Scene.GetIdentity(), m_Resolved.Buttons, input, m_Resolved.Generation);
    ReconcileSlider();
    if(m_PendingFocus&&input.Enabled){if(input.PointerPressed||m_Input.Captured())m_PendingFocus=0;else if(m_Input.SetFocus(m_Scene.GetIdentity(),m_PendingFocus,m_Resolved.Buttons))m_PendingFocus=0;}
    if (entering)
        m_Input.SetFocus(m_Scene.GetIdentity(), entryFocus, m_Resolved.Buttons);
    if (!input.Enabled)
    {
        if(!m_RetainPageEvents){m_Diagnostics.RetiredEvents+=m_Actions.size();m_Actions.clear();}
        m_PointerOver = false;
    }
    else
    {
        if (input.CancelDown){
            const auto before=m_Actions.size();
            m_Actions.erase(std::remove_if(m_Actions.begin(),m_Actions.end(),[](const Action &event){return event.Kind==UIEventKind::Activated;}),m_Actions.end());
            m_Diagnostics.RetiredEvents+=before-m_Actions.size();
        }
        Action a;
        while (m_Input.Consume(m_Scene.GetIdentity(), a))
        {
            if(m_ScrollParts.count(a.Entity))RouteScrollBar(a);else ActivateControl(std::move(a));
        }
        if(editing&&m_Slider.ID&&input.Adjust)AdjustSlider(input.Adjust);
        if(auto screen=Screen(m_Resolved.Scope);screen&&!editing&&m_Input.ConsumeCancel(m_Scene.GetIdentity(),m_Resolved.Scope)){
            m_ActionOwnsFrame=true;
            if(screen->Policy.Cancel==ScreenCancel::Close)RequestScreen(ScreenOperation::Close,screen->ID);
            else if(screen->Policy.Cancel==ScreenCancel::NotifyOwner){Action event;event.Kind=UIEventKind::Cancel;event.Entity=Live(screen->Policy.CancelTarget)?screen->Policy.CancelTarget:screen->ID;QueueEvent(std::move(event));}
        }
        PruneActions();
        m_ActionOwnsFrame |= !m_Actions.empty();
    }
    m_Diagnostics.InputGeneration = m_Resolved.Generation;
    m_Diagnostics.LastRoutingMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    WarnOverflow();
}
bool RuntimeUI::Consume(uint64_t entity, Action &action)
{
    if (std::none_of(m_Actions.begin(),m_Actions.end(),[&](const Action &a){return !a.Owner&&!a.Screen&&(!entity||a.Entity==entity);}))
        return false; // No candidate delivery: no dependency traversal.
    ResolveSurface(m_Resolved.Surface,
                   m_Resolved.PreviewOrphans); // catches unnotified native writes before delivery
    PruneActions();
    for (auto it = m_Actions.begin(); it != m_Actions.end(); ++it)
        if (!it->Owner && !it->Screen && (!entity || it->Entity == entity))
        {
            action = std::move(*it);
            m_Actions.erase(it);
            return true;
        }
    return false;
}
bool RuntimeUI::ConsumeCancel(uint64_t scope)
{
    if(Screen(scope)||!m_InputEnabled||!m_Input.ConsumeCancel(m_Scene.GetIdentity(),scope))return false;
    ResolveSurface(m_Resolved.Surface,m_Resolved.PreviewOrphans);
    return !Screen(scope)&&m_InputEnabled&&m_Resolved.Valid&&m_Input.Scope()==scope;
}
bool RuntimeUI::Bounds(uint64_t id, Rect &pixels) const
{
    auto n = m_Resolved.Find(id);
    if (!m_Resolved.Valid || !n || !n->LayoutResolved || !m_Scene.IsEntityValid(id))
        return false;
    pixels = n->PixelBounds;
    return true;
}
RuntimeUIDiagnostics RuntimeUI::Diagnostics() const
{
    auto result = m_Diagnostics;
    result.TextEdit=m_TextEdit.ID;if(m_TextEdit.ID){auto& field=m_TextStates.at(m_TextEdit.ID);result.TextCaret=field.Buffer.Selection().Caret.Scalar;result.TextAnchor=field.Buffer.Selection().Anchor.Scalar;result.TextError=field.Problem;result.TextHistoryBytes=field.Buffer.RetainedHistoryBytes();}result.TextEventBytes=TextQueueBytes();
    result.Tooltip=m_TipVisible?m_TipVisible:m_TipPreview;result.ScrollCapture=m_Bar.Owner;result.Dropdown=ActiveDropdown();
    result.SliderEdit=m_Slider.ID;result.SliderGesture=m_Slider.Terminal.Gesture;result.SliderPointer=m_Slider.Pointer;
    result.Focus = m_Input.Focused();
    result.Capture = m_Input.Captured();
    result.Hover = m_Input.Hovered();
    result.Modal = m_Resolved.Modal;
    result.Scope=m_Input.Scope();result.NavigationMisses=m_Input.NavigationMisses();
    if(auto screen=Screen(result.Scope))result.EventOwner=screen->Policy.EventOwner;
    result.PendingEvents=m_Actions.size();result.PendingScreens=m_ScreenRequests.size();
    result.InputEnabled = m_InputEnabled;
    result.GameplayBlocked = BlocksGameplay();
    result.PointerBlocked = BlocksPointer();
    return result;
}
namespace
{
glm::mat4 Transform(Rect r, float scale)
{
    return glm::translate(glm::mat4(1), glm::vec3((r.Position.x + r.Size.x * .5f) * scale,
                                                  -(r.Position.y + r.Size.y * .5f) * scale, 0)) *
           glm::scale(glm::mat4(1), glm::vec3(r.Size * scale, 1));
}
void Image(Rect bounds, float scale, const ResolvedSprite &sprite, const glm::vec4 &color, int id,
           const UINineSliceComponent *nine)
{
    auto draw = sprite;
    draw.Corners = SpriteCorners({.5f, .5f});
    draw.TilingFactor = 1;
    if (!nine)
    {
        Renderer2D::DrawSprite(Transform(bounds, scale), draw, color, id);
        return;
    }
    if (!sprite.Texture)
        throw std::runtime_error("Nine-slice requires a texture/sprite");
    const glm::vec2 sourceSize = sprite.PixelSize();
    if (!std::isfinite(nine->BorderScale) || nine->BorderScale <= 0)
        throw std::invalid_argument("Invalid nine-slice border scale");
    auto b = nine->Borders;
    for (int i = 0; i < 4; i++)
        if (!std::isfinite(b[i]) || b[i] < 0)
            throw std::runtime_error("Invalid nine-slice border");
    if (b.x + b.z > sourceSize.x || b.y + b.w > sourceSize.y)
        throw std::runtime_error("Nine-slice borders exceed source region");
    float horizontal =
              b.x + b.z ? std::min(1.f, bounds.Size.x / ((b.x + b.z) * nine->BorderScale)) : 1,
          vertical =
              b.y + b.w ? std::min(1.f, bounds.Size.y / ((b.y + b.w) * nine->BorderScale)) : 1;
    float x[] = {0, b.x * horizontal * nine->BorderScale,
                 bounds.Size.x - b.z * horizontal * nine->BorderScale, bounds.Size.x},
          y[] = {0, b.y * vertical * nine->BorderScale,
                 bounds.Size.y - b.w * vertical * nine->BorderScale, bounds.Size.y};
    float u[] = {0, b.x / sourceSize.x, 1 - b.z / sourceSize.x, 1},
          v[] = {1, 1 - b.y / sourceSize.y, b.w / sourceSize.y, 0};
    auto uv = [&](float u, float v) {
        return sprite.UV[0] + (sprite.UV[2] - sprite.UV[0]) * glm::vec2(u, v);
    };
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
        {
            draw.UV = {uv(u[col], v[row + 1]), uv(u[col + 1], v[row + 1]), uv(u[col + 1], v[row]),
                       uv(u[col], v[row])};
            Rect cell{bounds.Position + glm::vec2(x[col], y[row]),
                      {x[col + 1] - x[col], y[row + 1] - y[row]}};
            if (cell.Size.x > 0 && cell.Size.y > 0)
                Renderer2D::DrawSprite(Transform(cell, scale), draw, color, id);
        }
}
} // namespace
Ref<const Text::Layout> RuntimeUI::TextLayout(uint64_t id) const {
    auto it=m_Cache.find(id);auto n=m_Resolved.Find(id);
    if(!m_Resolved.Valid || !n || !n->LayoutResolved || it==m_Cache.end() || it->second.PaintGeneration!=m_Resolved.Generation)return {};
    if(auto field=m_TextStates.find(id);field!=m_TextStates.end()){
        if(!field->second.Prepared||!it->second.PaintLayout||it->second.PaintLayout->Source!=field->second.Buffer.Value())return {};
    }
    return it->second.PaintLayout;
}
bool RuntimeUI::ImageVisual(uint64_t id,UIImageVisual &result) const
{
    auto it=m_Cache.find(id);auto n=m_Resolved.Find(id);
    if(!m_Resolved.Valid||!n||!n->LayoutResolved||it==m_Cache.end()||!it->second.PaintImage.Generation||it->second.PaintImage.Generation!=m_Resolved.Generation)return false;
    result=it->second.PaintImage;return true;
}
void RuntimeUI::Draw()
{
    static_assert(std::tuple_size_v<decltype(m_Diagnostics.FlushReasons)> ==
                  size_t(Renderer2D::FlushReason::Count));
    m_Diagnostics.VisibleElements = m_Diagnostics.SubmittedElements = m_Diagnostics.DrawCalls =
        m_Diagnostics.UploadGroups = 0;
    m_Diagnostics.ClipTransitions = m_Diagnostics.PrimitiveTransitions =
        m_Diagnostics.FontTransitions = 0;
    m_Diagnostics.FlushReasons = {};
    m_Diagnostics.LastPrepareMilliseconds = m_Diagnostics.LastSubmitMilliseconds = 0;
    m_Diagnostics.GPUAvailable = false;
    m_Diagnostics.PreparedRebuilds = m_Diagnostics.PreparedReuses = 0;
    m_Diagnostics.PreparedBytes = m_PreparedBytes;
    if (!m_Resolved.Valid || m_Resolved.PainterOrder.empty() ||
        m_Resolved.Surface.Viewport.x <= 0 || m_Resolved.Surface.Viewport.y <= 0)
        return;
    // A host's pending world geometry must use its own camera and raster state.
    Renderer2D::EndScene();
    const auto paintStart = std::chrono::steady_clock::now();
    const auto before = Renderer2D::GetStats();
    const auto submitStart = before.SubmitMilliseconds;
    auto prior = RenderCommand::GetRasterState();
    auto state = prior;
    state.DepthTest = false;
    state.ClipEnabled = false;
    Camera camera(glm::ortho(0.f, m_Resolved.Surface.Viewport.x, -m_Resolved.Surface.Viewport.y,
                             0.f, -1.f, 1.f));
    Renderer2D::BeginOrderedScene(camera, glm::mat4(1));
    RenderCommand::SetRasterState(state);
    if (!m_GPUTimer)
        m_GPUTimer = GPUTimer::Create();
    if (m_GPUTimer)
        m_GPUTimer->Begin(++m_PaintSubmission);
    try
    {
        for (auto index : m_Resolved.PainterOrder)
        {
            const auto &n = m_Resolved.Elements[index];
            auto e = m_Scene.GetEntityByUUID(n.ID);
            if (!m_Scene.IsEntityValid(e.GetUUID()))
                continue;
            Rect visualBounds=n.Bounds;bool visualVisible=true;
            bool derived=ControlVisual(n.ID,visualBounds,visualVisible);if(!visualVisible)continue;
            auto paintClip=derived?FramebufferCoverage(Scaled(visualBounds.Intersect(n.InheritedClip),glm::vec2(n.Scale)),m_Resolved.Surface.Framebuffer/m_Resolved.Surface.Viewport,m_Resolved.Surface.Framebuffer):n.FramebufferClip;
            const auto p = paintClip.Position, z = paintClip.Size;
            if (z.x <= 0 || z.y <= 0)
                continue;
            ++m_Diagnostics.VisibleElements;
            auto geometryBefore = Renderer2D::GetGeometryCount();
            // Exact same inward-snapped coverage was used for hit eligibility.
            state.ClipRect = {prior.Viewport.x + int(p.x),
                              prior.Viewport.y + prior.Viewport.w - int(p.y + z.y), int(z.x),
                              int(z.y)};
            Renderer2D::SetPrimitiveClip(state.ClipRect);Renderer2D::SetPickingEnabled(!n.PassiveOverlay);
            auto tint = glm::vec4(1);
            if(auto part=m_ControlVisuals.find(n.ID);part!=m_ControlVisuals.end()){
                auto owner=m_Scene.GetEntityByUUID(part->second);auto root=m_Resolved.Find(part->second);
                if(owner&&root&&owner.HasComponent<UISliderComponent>()&&(!root->Enabled||!owner.GetComponent<UIButtonComponent>().Interactable))tint=glm::vec4(.4f,.4f,.4f,1);
            }
            const SpriteSource *overrideImage = nullptr;
            int stateIndex = 0;
            if (e.HasComponent<UIButtonComponent>())
            {
                auto &b = e.GetComponent<UIButtonComponent>();
                int index = n.ButtonIndex >= m_Resolved.Buttons.size()
                                ? 0
                                : int(m_Input.State(m_Resolved.Buttons[n.ButtonIndex]));
                if (m_Resolved.PreviewOrphans && !n.Canvas)
                    index = n.Enabled && b.Interactable ? int(ButtonState::Normal)
                                                        : int(ButtonState::Disabled);
                if (index == int(ButtonState::Focused) && !m_Resolved.Modal && !m_KeyboardOwns)
                    index = 0;
                tint = b.Colors[index];
                stateIndex = index;
                overrideImage = &b.Images[index];
            }
            const UINineSliceComponent *nine = e.HasComponent<UINineSliceComponent>()
                                                   ? &e.GetComponent<UINineSliceComponent>()
                                                   : nullptr;
            if (e.HasComponent<SpriteRendererComponent>())
            {
                auto &s = e.GetComponent<SpriteRendererComponent>();
                auto *sprite = m_Scene.RenderedSprite(e);
                if (overrideImage && overrideImage->index() != 0)
                {
                    if (!m_Scene.GetAssets())
                        throw std::runtime_error("UI image needs project assets");
                    const auto &assets = m_Scene.GetAssets();
                    auto &cache = m_Cache[n.ID];
                    if (!cache.Images[stateIndex] ||
                        cache.ImageEpochs[stateIndex] != assets->Epoch() ||
                        !(cache.ImageSources[stateIndex] == *overrideImage))
                    {
                        auto candidate = assets->Resolve(*overrideImage);
                        cache.Images[stateIndex] = std::move(candidate);
                        cache.ImageSources[stateIndex] = *overrideImage;
                        cache.ImageEpochs[stateIndex] = assets->Epoch();
                    }
                    sprite = cache.Images[stateIndex].get();
                }
                if (sprite)
                {
                    auto &rect = e.GetComponent<UIRectComponent>();
                    if (nine && rect.Fit != ImageFit::Stretch)
                        throw std::invalid_argument("Nine-slice requires Stretch image fit");
                    glm::vec2 sourceSize = sprite->PixelSize();
                    auto fitted = FitImage(
                        rect.Fit == ImageFit::Stretch ? visualBounds : visualBounds.Inset(rect.Padding),
                        sourceSize, rect.Fit, rect.ImageAlignment);
                    auto draw = *sprite;
                    // Sprite UVs use bottom-left coordinates; fit/crop uses top-left local units.
                    auto lo = fitted.UV.Position, hi = lo + fitted.UV.Size;
                    auto uv = [&](float u, float v) {
                        return sprite->UV[0] + (sprite->UV[2] - sprite->UV[0]) * glm::vec2(u, v);
                    };
                    draw.UV = {uv(lo.x, 1 - hi.y), uv(hi.x, 1 - hi.y), uv(hi.x, 1 - lo.y),
                               uv(lo.x, 1 - lo.y)};
                    if (fitted.Visual.Size.x > 0 && fitted.Visual.Size.y > 0)
                        Image(fitted.Visual, n.Scale, draw, s.Color * tint, int(uint32_t(e)), nine);
                    m_Cache[n.ID].PaintImage = {m_Resolved.Generation, sourceSize, rect.Fit,
                                                fitted};
                }
            }
            if(e.HasComponent<UIButtonComponent>()){
                auto &b=e.GetComponent<UIButtonComponent>();
                if((b.Kind==ControlKind::Toggle||b.Kind==ControlKind::Radio)&&!Decoration(n.ID,b.Indicator)){
                    Rect mark{n.Bounds.Position+glm::vec2(8,std::max(0.f,(n.Bounds.Size.y-16)*.5f)),glm::vec2(16)};
                    Renderer2D::DrawRect(Transform(mark,n.Scale),{1,1,1,1},int(uint32_t(e)));
                    bool selected=false;auto it=m_Values.find(b.Kind==ControlKind::Radio?b.Group:n.ID);
                    if(it!=m_Values.end())selected=b.Kind==ControlKind::Radio?it->second.Selected==n.ID:it->second.Checked;
                    if(selected)Renderer2D::DrawQuad(Transform(mark.Inset(glm::vec4(4)),n.Scale),glm::vec4(1),int(uint32_t(e)));
                }
            }
            // The former text/focus flush drew lines before glyphs; retain that layering.
            if (e.HasComponent<UIButtonComponent>())
            {
                if (n.FocusEligible && m_Input.Focused() == n.ID &&
                    (m_Resolved.Modal || m_KeyboardOwns))
                {
                    auto inset = n.Bounds.Inset(glm::vec4(2));
                    Renderer2D::DrawRect(Transform(inset, n.Scale), m_Slider.ID==n.ID?glm::vec4(.3f,1,1,1):glm::vec4(1,.9f,.65f,1),
                                         int(uint32_t(e)));
                }
            }
            if(e.HasComponent<UITextEntryComponent>())DrawTextField(n,e,prior);
            else if (e.HasComponent<TextComponent>())
            {
                auto &t = e.GetComponent<TextComponent>();
                const auto &displayText=DisplayText(e);
                const UITextStyleComponent defaults;
                const auto &style = e.HasComponent<UITextStyleComponent>()
                                        ? e.GetComponent<UITextStyleComponent>()
                                        : defaults;
                if (!std::isfinite(style.Size) || style.Size <= 0 || style.Size > 512 ||
                    style.Horizontal < Align::Start || style.Horizontal > Align::End ||
                    style.Vertical < Align::Start || style.Vertical > Align::End ||
                    unsigned(style.Overflow)>1 || style.MaxLines>4096 || !std::isfinite(t.Kerning) || !std::isfinite(t.LineSpacing))
                    throw std::invalid_argument("Invalid native UI text style/spacing");
                auto &cache = m_Cache[e.GetUUID()];
                const auto assets = m_Scene.GetAssets();
                const auto epoch = assets ? assets->Epoch() : 1;
                if (!cache.FontAsset || cache.Selection != t.Font || cache.Epoch != epoch)
                {
                    cache.FontAsset =
                        ResolveTextFont(t,assets);
                    cache.Selection = t.Font;
                    cache.Epoch = epoch;
                    cache.Width = -1;
                    cache.PreparedValid = false;
                    cache.MeasureWidth = {{-2, -2}};
                    cache.MeasureLines[0].clear();cache.MeasureLines[1].clear();cache.Lines.clear();cache.PaintLayout.reset();
                }
                auto content = n.Bounds.Inset(e.GetComponent<UIRectComponent>().Padding);
                if(style.Overflow==Text::Overflow::Ellipsis) {
                    // Ellipsis owns a finite content box; ink overhang is clipped
                    // there too. Images on this same element retain their clip.
                    auto logical=n.Clip.Intersect(content);
                    auto ratio=m_Resolved.Surface.Framebuffer/m_Resolved.Surface.Viewport;
                    auto coverage=FramebufferCoverage({logical.Position*n.Scale,logical.Size*n.Scale},ratio,m_Resolved.Surface.Framebuffer);
                    const auto a=coverage.Position,size=coverage.Size;
                    Renderer2D::SetPrimitiveClip({prior.Viewport.x+int(a.x),prior.Viewport.y+prior.Viewport.w-int(a.y+size.y),int(size.x),int(size.y)});
                }
                float width = content.Size.x / style.Size;
                float textHeight=style.Overflow==Text::Overflow::Ellipsis?content.Size.y/style.Size:-1;
                if (!cache.PaintLayout || cache.Lines.size()!=cache.PaintLayout->Lines.size() || cache.Text != displayText || cache.Width != width ||
                    cache.Kerning != t.Kerning || cache.LineSpacing!=t.LineSpacing || cache.Wrap != style.Wrap || cache.Overflow!=style.Overflow || cache.MaxLines!=style.MaxLines || cache.Height!=textHeight)
                {
                    cache.PreparedValid=false;
                    bool measured=false;
                    for(size_t slot=0;slot<2;++slot)if(style.Overflow==Text::Overflow::Clip && cache.MeasureOverflow[slot]==style.Overflow && cache.MeasureMaxLines[slot]==style.MaxLines && !cache.MeasureLines[slot].empty() && cache.MeasureLines[slot].front().Result->Font==cache.FontAsset->GetCPUFace() && cache.MeasureText[slot]==displayText &&
                        cache.MeasureKerning[slot]==t.Kerning && cache.MeasureSpacing[slot]==t.LineSpacing && cache.MeasureWrap[slot]==style.Wrap &&
                        (!style.Wrap || cache.MeasureWidth[slot]==width)) {
                        cache.Lines=cache.MeasureLines[slot];cache.PaintLayout=cache.Lines.front().Result;measured=true;break;
                    }
                    if(!measured)cache.PaintLayout=Renderer2D::LayoutText(displayText, cache.FontAsset, width, style.Wrap,
                                           t.Kerning, cache.Lines,t.LineSpacing,style.Overflow,style.MaxLines,textHeight);
                    cache.PreparedValid = false;
                    cache.Text = displayText;
                    cache.Width = width;
                    cache.Kerning = t.Kerning;
                    cache.LineSpacing=t.LineSpacing;
                    cache.Wrap = style.Wrap;
                    cache.Overflow=style.Overflow;cache.MaxLines=style.MaxLines;cache.Height=textHeight;
                }
                // Position/tint are live draw values. Only normalized glyph geometry is cached.
                const glm::vec2 normalizedContent = content.Size / style.Size;
                const bool changed =
                    !cache.PreparedValid || cache.PreparedContent != normalizedContent ||
                    cache.PreparedSize != style.Size || cache.PreparedSpacing != t.LineSpacing ||
                    cache.PreparedHorizontal != style.Horizontal ||
                    cache.PreparedVertical != style.Vertical;
                constexpr size_t budget = 16 * 1024 * 1024, maxTextBytes = 4096;
                const size_t priorBytes = cache.Glyphs.capacity() * sizeof(Renderer2D::TextGlyph);
                const size_t wanted = std::max(cache.Glyphs.capacity(), cache.Lines.empty()?size_t(0):cache.Lines.front().Result->Glyphs.size()) *
                                      sizeof(Renderer2D::TextGlyph);
                const bool retain = displayText.size() <= maxTextBytes &&
                                    m_PreparedBytes - priorBytes + wanted <= budget;
                if (!retain)
                {
                    m_PreparedBytes -= priorBytes;
                    std::vector<Renderer2D::TextGlyph>().swap(cache.Glyphs);
                    cache.PreparedValid = false;
                }
                auto face=cache.FontAsset->GetCPUFace();
                const float normalized=1/face->HeightEm();
                const float lineHeight=face->LineHeightEm()*normalized+t.LineSpacing;
                const float height = cache.Lines.empty()?0:1.f + (cache.Lines.size() - 1) * lineHeight;
                auto visitLines = [&](auto emit) {
                    float y = style.Vertical == Align::Center ? (normalizedContent.y - height) * .5f
                              : style.Vertical == Align::End  ? normalizedContent.y - height
                                                              : 0;
                    y += face->AscenderEm() * normalized;
                    for (const auto &line : cache.Lines)
                    {
                        const float x = style.Horizontal == Align::Center
                                            ? (normalizedContent.x - line.Width) * .5f
                                        : style.Horizontal == Align::End
                                            ? normalizedContent.x - line.Width
                                            : 0;
                        emit(line, glm::vec2(x, -y));
                        y += lineHeight;
                    }
                };
                const auto transform =
                    glm::translate(glm::mat4(1), glm::vec3(content.Position.x * n.Scale,
                                                           -content.Position.y * n.Scale, 0)) *
                    glm::scale(glm::mat4(1), glm::vec3(style.Size * n.Scale));
                if (retain)
                {
                    if (changed)
                    {
                        cache.PreparedValid = false;
                        cache.Glyphs.clear();
                        cache.Glyphs.reserve(cache.Lines.empty()?0:cache.Lines.front().Result->Glyphs.size());
                        m_PreparedBytes = m_PreparedBytes - priorBytes +
                                          cache.Glyphs.capacity() * sizeof(Renderer2D::TextGlyph);
                        visitLines([&](const auto &line, glm::vec2 offset) {
                            Renderer2D::AppendTextLineGlyphs(line, cache.FontAsset, cache.Glyphs, offset);
                        });
                        cache.PreparedContent = normalizedContent;
                        cache.PreparedSize = style.Size;
                        cache.PreparedSpacing = t.LineSpacing;
                        cache.PreparedHorizontal = style.Horizontal;
                        cache.PreparedVertical = style.Vertical;
                        cache.PreparedValid = true;
                        ++m_Diagnostics.PreparedRebuilds;
                    }
                    else
                        ++m_Diagnostics.PreparedReuses;
                    Renderer2D::DrawTextGlyphs(cache.Glyphs, cache.FontAsset, transform,
                                               t.Color * tint, int(uint32_t(e)));
                }
                else
                {
                    ++m_Diagnostics.PreparedRebuilds;
                    visitLines([&](const auto &line, glm::vec2 offset) {
                        m_ScratchGlyphs.clear();
                        Renderer2D::AppendTextLineGlyphs(line,cache.FontAsset,m_ScratchGlyphs,offset);
                        Renderer2D::DrawTextGlyphs(m_ScratchGlyphs,cache.FontAsset,transform,t.Color*tint,int(uint32_t(e)));
                        m_ScratchGlyphs.clear(); // no unaccounted page pins across frames
                    });
                }
                cache.PaintGeneration=m_Resolved.Generation;
                m_Diagnostics.PreparedBytes = m_PreparedBytes;
            }
            if (Renderer2D::GetGeometryCount() != geometryBefore)
                ++m_Diagnostics.SubmittedElements;
        }
        Renderer2D::EndScene();
        if (m_GPUTimer)
            m_GPUTimer->End();
    }
    catch (...)
    {
        m_ScratchGlyphs.clear();
        Renderer2D::EndScene();
        if (m_GPUTimer)
            m_GPUTimer->End();
        RenderCommand::SetRasterState(prior);
        throw;
    }
    RenderCommand::SetRasterState(prior);
    const auto after = Renderer2D::GetStats();
    m_Diagnostics.DrawCalls = after.DrawCalls - before.DrawCalls;
    m_Diagnostics.ClipTransitions = after.ClipTransitions - before.ClipTransitions;
    m_Diagnostics.PrimitiveTransitions = after.PrimitiveTransitions - before.PrimitiveTransitions;
    m_Diagnostics.FontTransitions = after.FontTransitions - before.FontTransitions;
    for (size_t i = 0; i < m_Diagnostics.FlushReasons.size(); ++i)
    {
        m_Diagnostics.FlushReasons[i] = after.Flushes[i] - before.Flushes[i];
        m_Diagnostics.UploadGroups += m_Diagnostics.FlushReasons[i];
    }
    m_Diagnostics.PaintSubmission = m_PaintSubmission;
    if (m_GPUTimer)
    {
        auto result = m_GPUTimer->Latest();
        m_Diagnostics.GPUAvailable = result.Available;
        m_Diagnostics.LastGPUMilliseconds = result.Milliseconds;
        m_Diagnostics.GPUSubmission = result.Submission;
    }
    m_Diagnostics.LastSubmitMilliseconds = Renderer2D::GetStats().SubmitMilliseconds - submitStart;
    m_Diagnostics.LastPrepareMilliseconds = std::max(
        0., std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - paintStart)
                    .count() -
                m_Diagnostics.LastSubmitMilliseconds);
}
} // namespace Hazel
