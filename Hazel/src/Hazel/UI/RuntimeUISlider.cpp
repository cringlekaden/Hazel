#include "hzpch.h"

#include "RuntimeUI.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
size_t RuntimeUI::ReservedSliderSlots() const
{
    if (!m_Slider.ID)
        return 0;
    bool preview = std::any_of(m_Actions.begin(), m_Actions.end(), [&](const Action &a) {
        return a.Gesture == m_Slider.Terminal.Gesture && a.Kind == UIEventKind::SliderChanged;
    });
    return preview ? 1 : 2;
}
SliderGeometry RuntimeUI::SliderGeometryFor(uint64_t id) const
{
    auto e = m_Scene.GetEntityByUUID(id);
    auto root = m_Resolved.Find(id);
    auto it = m_Values.find(id);
    if (!e || !root || !root->LayoutResolved || it == m_Values.end() ||
        !e.HasComponent<UIRectComponent>())
        throw std::invalid_argument("Slider has no arranged root");
    auto &v = it->second;
    auto track = m_Resolved.Find(v.Track), thumb = m_Resolved.Find(v.Thumb);
    if (!Decoration(id, v.Track) || !Decoration(id, v.Thumb) || !track || !thumb ||
        !track->LayoutResolved || !thumb->LayoutResolved)
        throw std::invalid_argument(
            "Slider needs live arranged direct track/thumb Sprite/Rect leaves");
    auto t = m_Scene.GetEntityByUUID(v.Track), h = m_Scene.GetEntityByUUID(v.Thumb);
    if (!track->Visible || !thumb->Visible || !track->Enabled || !thumb->Enabled ||
        !t.GetComponent<UIRectComponent>().Visible || !h.GetComponent<UIRectComponent>().Visible)
        throw std::invalid_argument("Slider track/thumb is hidden or disabled");
    if (v.Fill)
    {
        auto fill = m_Resolved.Find(v.Fill);
        if (!Decoration(id, v.Fill) || !fill || !fill->LayoutResolved || !fill->Visible ||
            !fill->Enabled)
            throw std::invalid_argument("Slider assigned fill is missing, hidden or disabled; "
                                        "repair or clear its reference");
    }
    return ArrangeSlider(root->Bounds.Inset(e.GetComponent<UIRectComponent>().Padding),
                         track->Bounds, thumb->Bounds.Size,
                         (v.Value - v.Range.Minimum) / (v.Range.Maximum - v.Range.Minimum),
                         v.Direction);
}
void RuntimeUI::SyncSliders()
{
    for (auto id : m_RectIDs)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        if (!e.HasComponent<UISliderComponent>())
            continue;
        if (!e.HasComponent<UIButtonComponent>() ||
            e.GetComponent<UIButtonComponent>().Kind != ControlKind::Slider ||
            e.HasComponent<UIProgressComponent>() || e.HasComponent<UIRadioGroupComponent>())
        {
            if (m_Slider.ID == id)
                EndSlider(false, true);
            m_Values.erase(id);
            ControlProblem(
                "Slider needs Slider Button behavior without competing value components");
            continue;
        }
        const auto &s = e.GetComponent<UISliderComponent>();
        try
        {
            s.Validate();
            if (!std::isfinite(s.InitialValue) || unsigned(s.Direction) > 3)
                throw std::invalid_argument("Invalid slider initial value/direction");
        }
        catch (const std::exception &error)
        {
            if (m_Slider.ID == id)
                EndSlider(false, true);
            auto old = m_Values.find(id);
            if (old != m_Values.end() &&
                (old->second.Lifetime != m_Scene.GetAuthoringLifetime(id) ||
                 old->second.Component !=
                     m_Scene.GetAuthoringComponentVersion(id, typeid(UISliderComponent))))
                m_Values.erase(old);
            ControlProblem("Slider #" + std::to_string(id) + ": " + error.what());
            continue;
        }
        auto lifetime = m_Scene.GetAuthoringLifetime(id),
             component = m_Scene.GetAuthoringComponentVersion(id, typeid(UISliderComponent));
        auto found = m_Values.find(id);
        bool changed =
            found == m_Values.end() || found->second.Lifetime != lifetime ||
            found->second.Component != component || found->second.Range.Minimum != s.Minimum ||
            found->second.Range.Maximum != s.Maximum || found->second.Range.Step != s.Step ||
            found->second.Initial != s.InitialValue;
        if (changed && m_Slider.ID == id)
            EndSlider(false, true);
        auto &v = m_Values[id];
        if (changed || !m_Scene.IsRunning())
        {
            auto revision = v.Revision + 1;
            v = {};
            v.Lifetime = lifetime;
            v.Component = component;
            v.Kind = ControlKind::Slider;
            v.Range = s;
            v.Initial = s.InitialValue;
            v.Value = s.Quantize(s.InitialValue);
            v.Revision = revision;
        }
        v.Track = s.Track;
        v.Fill = s.Fill;
        v.Thumb = s.Thumb;
        v.Direction = s.Direction;
        for (auto part : {s.Track, s.Fill, s.Thumb})
            if (part)
            {
                if (Decoration(id, part))
                    m_ControlVisuals.insert_or_assign(part, id);
                else
                    ControlProblem("Slider #" + std::to_string(id) + ": broken visual #" +
                                   std::to_string(part));
            }
        if (!s.Track || !s.Thumb || s.Track == s.Thumb ||
            (s.Fill && (s.Fill == s.Track || s.Fill == s.Thumb)))
            ControlProblem("Slider needs distinct track/thumb and optional fill leaves");
    }
    ReconcileSlider();
}
float RuntimeUI::SliderValue(uint64_t id)
{
    auto e = m_Scene.GetEntityByUUID(id);
    auto it = m_Values.find(id);
    if (!e || !e.HasComponent<UISliderComponent>() || it == m_Values.end() ||
        it->second.Kind != ControlKind::Slider)
        throw std::invalid_argument("Slider has no accepted range/value");
    return it->second.Value;
}
void RuntimeUI::SetSliderValue(uint64_t id, float value)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping() || !std::isfinite(value))
        throw std::invalid_argument("Slider setter requires Play and a finite value");
    (void)SliderValue(id);
    auto quantized = m_Values.at(id).Range.Quantize(value);
    if (m_Slider.ID == id)
        EndSlider(false, true);
    auto &v = m_Values.at(id);
    v.Value = quantized;
    ++v.Revision;
}
bool RuntimeUI::BeginSlider(uint64_t id, bool pointer, glm::vec2 point)
{
    const bool takeover = m_Slider.ID == id && pointer && !m_Slider.Pointer;
    if (m_Slider.ID && !takeover)
    {
        if (m_Slider.ID == id && !pointer)
            EndSlider(false);
        return false;
    }
    try
    {
        if (!Live(id) || (!takeover && m_Actions.size() + ReservedDropdownSlots() + 2 > InputRouter::MaxActions))
            throw std::invalid_argument(
                "Slider edit needs two free event slots and an eligible control");
        auto geometry = SliderGeometryFor(id);
        auto n = m_Resolved.Find(id);
        glm::vec2 ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
        glm::vec2 logical = point / (ratio * n->Scale);
        if (pointer && !geometry.Track.Contains(logical) && !geometry.Thumb.Contains(logical))
        {
            m_Input.CancelCapture();
            return false;
        }
        if (pointer && geometry.Travel <= 0)
        {
            m_Input.CancelCapture();
            ControlProblem("Slider track has zero usable travel");
            return false;
        }
        auto &v = m_Values.at(id);
        if (!takeover)
        {
            Action terminal;
            terminal.Entity = terminal.Control = id;
            terminal.Kind = UIEventKind::SliderCommitted;
            auto e = m_Scene.GetEntityByUUID(id);
            terminal.Name = e.GetComponent<UIButtonComponent>().Action;
            terminal.SourceLifetime = terminal.ControlLifetime = m_Scene.GetAuthoringLifetime(id);
            terminal.SourceComponent =
                m_Scene.GetAuthoringComponentVersion(id, typeid(UIButtonComponent));
            terminal.ControlComponent =
                m_Scene.GetAuthoringComponentVersion(id, typeid(UISliderComponent));
            if (!StampEvent(terminal))
                return false;
            terminal.Gesture = ++m_NextGesture;
            terminal.StartValue = v.Value;
            m_Slider = {};
            m_Slider.ID = id;
            m_Slider.Revision = v.Revision;
            m_Slider.Track = v.Track;
            m_Slider.Fill = v.Fill;
            m_Slider.Thumb = v.Thumb;
            m_Slider.Start = v.Value;
            m_Slider.Pointer = pointer;
            m_Slider.Scale = n->Scale;
            m_Slider.Viewport = m_Resolved.Surface.Viewport;
            m_Slider.Framebuffer = m_Resolved.Surface.Framebuffer;
            m_Slider.Geometry = geometry;
            m_Slider.Terminal = std::move(terminal);
        }
        else
        {
            m_Slider.Pointer = true;
            m_Slider.Geometry = geometry;
        }
        if (pointer)
        {
            if (geometry.Thumb.Contains(logical))
                m_Slider.Grab = logical[geometry.Axis] - (geometry.Thumb.Position[geometry.Axis] +
                                                          geometry.Thumb.Size[geometry.Axis] * .5f);
            else
                ChangeSlider(v.Range.Minimum +
                             geometry.Position(logical) * (v.Range.Maximum - v.Range.Minimum));
        }
        m_KeyboardOwns = true;
        return true;
    }
    catch (const std::exception &error)
    {
        ControlProblem(error.what());
        m_Input.CancelCapture();
        return false;
    }
}
bool RuntimeUI::ChangeSlider(float value)
{
    if (!m_Slider.ID)
        return false;
    auto &v = m_Values.at(m_Slider.ID);
    if (v.Revision != m_Slider.Revision)
    {
        EndSlider(false, true);
        return false;
    }
    value = v.Range.Quantize(value);
    if (value == v.Value)
        return true;
    auto found = std::find_if(m_Actions.begin(), m_Actions.end(), [&](const Action &a) {
        return a.Gesture == m_Slider.Terminal.Gesture && a.Kind == UIEventKind::SliderChanged;
    });
    if (found != m_Actions.end())
    {
        auto preview = std::move(*found);
        m_Actions.erase(found);
        preview.Value = value;
        preview.Sequence = ++m_EventSequence;
        m_Actions.push_back(std::move(preview));
    }
    else
    {
        Action preview = m_Slider.Terminal;
        preview.Kind = UIEventKind::SliderChanged;
        preview.PreviousValue = v.Value;
        preview.Value = value;
        preview.Sequence = ++m_EventSequence;
        if (m_Actions.size() + ReservedDropdownSlots() + 1 >= InputRouter::MaxActions)
            throw std::logic_error("Slider reservation invariant violated");
        m_Actions.push_back(std::move(preview));
    }
    v.Value = value;
    ++v.Revision;
    m_Slider.Revision = v.Revision;
    return true;
}
void RuntimeUI::EndSlider(bool cancel, bool interrupted, bool deliver)
{
    if (!m_Slider.ID)
        return;
    auto it = m_Values.find(m_Slider.ID);
    if (cancel && !interrupted && it != m_Values.end() && it->second.Revision == m_Slider.Revision)
        ChangeSlider(m_Slider.Start);
    auto ending = std::move(m_Slider);
    m_Slider = {};
    m_Input.CancelCapture();
    m_SliderNeutral = true;
    if (!deliver)
        return;
    auto event = std::move(ending.Terminal);
    event.Kind =
        cancel || interrupted ? UIEventKind::SliderCancelled : UIEventKind::SliderCommitted;
    event.Interrupted = interrupted;
    event.PreviousValue = ending.Start;
    event.Value = it == m_Values.end() ? ending.Start : it->second.Value;
    if (EventValid(event))
    {
        if (m_Actions.size() + ReservedDropdownSlots() >= InputRouter::MaxActions)
            throw std::logic_error("Slider terminal reservation violated");
        event.Sequence = ++m_EventSequence;
        m_Actions.push_back(std::move(event));
    }
    else
        ++m_Diagnostics.RetiredEvents;
}
void RuntimeUI::ReconcileSlider()
{
    if (!m_Slider.ID)
        return;
    try
    {
        if (!m_Values.count(m_Slider.ID) || !EventValid(m_Slider.Terminal) || !Live(m_Slider.ID) ||
            m_Values.at(m_Slider.ID).Track != m_Slider.Track ||
            m_Values.at(m_Slider.ID).Fill != m_Slider.Fill ||
            m_Values.at(m_Slider.ID).Thumb != m_Slider.Thumb || m_Input.Focused() != m_Slider.ID ||
            m_Resolved.Find(m_Slider.ID)->Scale != m_Slider.Scale ||
            m_Resolved.Surface.Viewport != m_Slider.Viewport ||
            m_Resolved.Surface.Framebuffer != m_Slider.Framebuffer ||
            !(SliderGeometryFor(m_Slider.ID) == m_Slider.Geometry))
            EndSlider(false, true);
    }
    catch (const std::exception &error)
    {
        ControlProblem(error.what());
        EndSlider(false, true);
    }
}
void RuntimeUI::RouteSlider(const Action &a)
{
    if (a.Phase == RoutedPhase::Begin)
    {
        if (!BeginSlider(a.Entity, true, a.Point))
            m_Input.CancelCapture();
        return;
    }
    if (a.Phase == RoutedPhase::Cancel)
    {
        EndSlider(true);
        return;
    }
    if (!m_Slider.ID || m_Slider.ID != a.Entity || !m_Slider.Pointer)
        return;
    if (a.Phase == RoutedPhase::Move || a.Phase == RoutedPhase::End)
    {
        auto n = m_Resolved.Find(a.Entity);
        auto ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
        auto point = a.Point / (ratio * n->Scale);
        auto &v = m_Values.at(a.Entity);
        ChangeSlider(v.Range.Minimum + m_Slider.Geometry.Position(point, m_Slider.Grab) *
                                           (v.Range.Maximum - v.Range.Minimum));
        if (a.Phase == RoutedPhase::End)
            EndSlider(false);
    }
}
void RuntimeUI::AdjustSlider(int delta)
{
    if (!m_Slider.ID || m_Slider.Pointer || !delta)
        return;
    auto &v = m_Values.at(m_Slider.ID);
    auto index = v.Range.Index(v.Value), last = v.Range.LastIndex();
    if (delta > 0)
    {
        if (index < last)
            ++index;
    }
    else if (index)
        --index;
    ChangeSlider(v.Range.At(index));
}
} // namespace Hazel
