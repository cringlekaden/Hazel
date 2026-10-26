#include "hzpch.h"
#include "RuntimeUI.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Hazel/Utils/UTF8.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
namespace
{
Text::EditLimits EntryLimits(const UITextEntryComponent &c)
{
    return {c.MaxBytes, c.MaxGraphemes, c.MaxLines, c.Multiline, c.ReadOnly};
}
void ValidateEntry(const UITextEntryComponent &c)
{
    Text::EditBuffer::Normalize("", EntryLimits(c));
    if (c.Placeholder.size() > 1024)
        throw std::invalid_argument("Text placeholder exceeds 1024 bytes");
    UTF8::Validate(c.Placeholder);
    for (int a = 0; a < 4; ++a)
        if (!std::isfinite(c.CaretColor[a]) || c.CaretColor[a] < 0 || c.CaretColor[a] > 1 ||
            !std::isfinite(c.SelectionColor[a]) || c.SelectionColor[a] < 0 || c.SelectionColor[a] > 1)
            throw std::invalid_argument("Text-entry colors must be finite 0..1");
}
} // namespace
void RuntimeUI::ValidateTextFields(uint64_t target) const
{
    size_t count = 0;
    for (auto h : m_Scene.GetAllEntitiesWith<UITextEntryComponent>())
    {
        Entity e(h, &m_Scene);
        if (!m_Scene.IsEntityValid(e.GetUUID()))
            continue;
        if (++count > 16)
            throw std::invalid_argument("Text-entry declaration limit exceeded (16)");
        if (target && target != uint64_t(e.GetUUID()))
            continue;
        ValidateEntry(e.GetComponent<UITextEntryComponent>());
        bool extraChildren=false;
        for(auto child:m_Scene.GetChildrenView(e.GetUUID()))
            extraChildren|=!e.HasComponent<UITooltipComponent>()||e.GetComponent<UITooltipComponent>().Body!=uint64_t(child);
        if (!e.HasComponent<UIRectComponent>() || !e.HasComponent<TextComponent>() ||
            !e.HasComponent<UIButtonComponent>() ||
            e.GetComponent<UIButtonComponent>().Kind != ControlKind::TextEntry ||
            e.HasComponent<UILayoutComponent>() || e.HasComponent<UIScrollViewComponent>() ||
            e.HasComponent<UICanvasComponent>() || e.HasComponent<UISliderComponent>() ||
            e.HasComponent<UIRadioGroupComponent>() || extraChildren)
            throw std::invalid_argument("Text Entry needs a Rect/Text/Button with Text Entry behavior (only an owned tooltip body may be a child) "
                                        "and no competing container/value components");
        const auto &r = e.GetComponent<UIRectComponent>();
        if (r.WidthMode == SizeMode::Content || r.HeightMode == SizeMode::Content)
            throw std::invalid_argument(
                "Text-entry viewport requires Fixed/Fill dimensions; text scrolls inside its padded box");
        auto style = e.HasComponent<UITextStyleComponent>() ? e.GetComponent<UITextStyleComponent>()
                                                            : UITextStyleComponent{};
        if (style.Horizontal != Align::Start || style.Vertical != Align::Start || style.MaxLines ||
            style.Overflow != Text::Overflow::Clip)
            throw std::invalid_argument("Text Entry owns line limits and scrolling; use Start alignment, "
                                        "Clip and unlimited display Max lines");
        Text::EditBuffer::Normalize(e.GetComponent<TextComponent>().TextString,
                                    EntryLimits(e.GetComponent<UITextEntryComponent>()));
    }
}
void RuntimeUI::SyncTextFields()
{
    for (auto it = m_TextStates.begin(); it != m_TextStates.end();)
    {
        auto e = m_Scene.GetEntityByUUID(it->first);
        if (!e || !e.HasComponent<UITextEntryComponent>())
        {
            if (m_TextEdit.ID == it->first)
                EndTextField(false, true, false);
            it = m_TextStates.erase(it);
        }
        else
            ++it;
    }
    size_t count = 0;
    for (auto id : m_RectIDs)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        if (!e.HasComponent<UITextEntryComponent>())
            continue;
        if (++count > 16)
            throw std::invalid_argument("Text-entry declaration limit exceeded (16)");
        if (!e.HasComponent<TextComponent>() || !e.HasComponent<UIButtonComponent>() ||
            e.GetComponent<UIButtonComponent>().Kind != ControlKind::TextEntry)
        {
            ControlProblem("Text Entry #" + std::to_string(id) +
                           ": needs Text and Text Entry Button behavior");
            continue;
        }
        const auto &c = e.GetComponent<UITextEntryComponent>();
        auto &v = m_TextStates[id];
        auto life = m_Scene.GetAuthoringLifetime(id),
             version = m_Scene.GetAuthoringComponentVersion(id, typeid(UITextEntryComponent));
        try
        {
            ValidateEntry(c);
            const auto &initial = e.GetComponent<TextComponent>().TextString;
            bool reset = v.Lifetime != life || v.Component != version || v.Initial != initial ||
                         v.Config.MaxBytes != c.MaxBytes || v.Config.MaxGraphemes != c.MaxGraphemes ||
                         v.Config.MaxLines != c.MaxLines || v.Config.Multiline != c.Multiline ||
                         v.Config.ReadOnly != c.ReadOnly || !m_Scene.IsRunning();
            if (reset)
            {
                auto accepted = Text::EditBuffer(initial, EntryLimits(c));
                if (m_TextEdit.ID == id)
                    EndTextField(false, true, false);
                m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                               [&](auto &a) {
                                                   return a.Entity == id &&
                                                          a.Kind >= UIEventKind::TextChanged;
                                               }),
                                m_Actions.end());
                v.Buffer = std::move(accepted);
                v.Initial = initial;
                v.Lifetime = life;
                v.Component = version;
                v.Scroll = {};
                ++v.Revision;
                v.Prepared = false;
            }
            v.Config = c;
            v.Valid = true;
        }
        catch (const std::exception &error)
        {
            v.Valid = false;
            v.Problem = error.what();
            ControlProblem("Text Entry #" + std::to_string(id) + ": " + v.Problem);
        }
    }
}
std::string RuntimeUI::EntryValue(uint64_t id)
{
    auto it = m_TextStates.find(id);
    if (it == m_TextStates.end() || !it->second.Valid)
        throw std::invalid_argument("Text Entry has no accepted configuration");
    return it->second.Buffer.Value();
}
void RuntimeUI::SetEntryValue(uint64_t id, const std::string &value)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping())
        throw std::invalid_argument("Text Entry setter requires Play");
    (void)EntryValue(id);
    auto &v = m_TextStates.at(id);
    Text::EditBuffer accepted(value, EntryLimits(v.Config));
    if (m_TextEdit.ID == id)
        EndTextField(false, true, true);
    v.Buffer = std::move(accepted);
    ++v.Revision;
    v.Prepared = false;
    v.Scroll = {};
}
void RuntimeUI::PrepareTextField(uint64_t id)
{
    auto found = m_TextStates.find(id);
    auto e = m_Scene.GetEntityByUUID(id);
    auto n = m_Resolved.Find(id);
    if (found == m_TextStates.end() || !e || !n || !n->LayoutResolved)
        return;
    auto &v = found->second;
    auto style = e.HasComponent<UITextStyleComponent>() ? e.GetComponent<UITextStyleComponent>()
                                                        : UITextStyleComponent{};
    auto &cache = m_Cache[id];
    auto &text = e.GetComponent<TextComponent>();
    auto assets = m_Scene.GetAssets();
    auto epoch = assets ? assets->Epoch() : 1;
    auto content = n->Bounds.Inset(e.GetComponent<UIRectComponent>().Padding);
    if (content.Size.x <= 0 || content.Size.y <= 0)
    {
        v.Layout.reset();
        v.Paint.reset();
        v.Prepared = false;
        v.Problem = "Text entry has no usable padded content area";
        return;
    }
    float width = v.Config.Multiline && style.Wrap ? content.Size.x / style.Size : -1;
    if (!v.Prepared || v.PreparedValue != v.Buffer.Value() || v.Width != width || v.Size != style.Size ||
        v.Tracking != text.Kerning || v.Spacing != text.LineSpacing || v.Epoch != epoch ||
        v.Placeholder != v.Config.Placeholder || cache.Selection != text.Font)
    {
        v.Prepared = true;
        v.PreparedValue = v.Buffer.Value();
        v.Width = width;
        v.Size = style.Size;
        v.Tracking = text.Kerning;
        v.Spacing = text.LineSpacing;
        v.Epoch = epoch;
        v.PaintFailed = false;
        v.Problem.clear();
        cache.PreparedValid = false;
        cache.Selection=text.Font;
        try
        {
            v.FontAsset = ResolveTextFont(text, assets);
            cache.FontAsset = v.FontAsset;
            cache.Selection = text.Font;
            cache.Epoch = epoch;
            auto layout = v.FontAsset->Layout(v.Buffer.Value(), width, v.Config.Multiline, text.Kerning,
                                              text.LineSpacing);
            Text::EditGeometry geometry;
            geometry.Build(layout);
            v.Layout = std::move(layout);
            v.Geometry = std::move(geometry);
            v.Paint = v.Buffer.Value().empty() && !v.Config.Placeholder.empty()
                          ? v.FontAsset->Layout(v.Config.Placeholder, width, v.Config.Multiline, text.Kerning,
                                                text.LineSpacing)
                          : v.Layout;
            ++v.LayoutRevision;
            v.Placeholder = v.Config.Placeholder;
            auto extent = glm::vec2(v.Layout->Width, v.Layout->Height);
            v.Scroll = glm::clamp(v.Scroll, glm::vec2(0),
                                  glm::max(glm::vec2(0), extent - content.Size / style.Size));
            if (m_TextEdit.ID == id)
                RevealTextCaret(id);
            if (v.Layout->MissingGlyphs)
                v.Problem = "Source glyphs missing: replacement glyphs retain editable text";
        }
        catch (const std::exception &error)
        {
            v.Layout.reset();
            v.Paint.reset();
            v.Geometry = {};
            cache.PaintLayout.reset();
            cache.PaintGeneration = 0;
            v.Problem = error.what();
            v.PaintFailed = true;
            ControlProblem("Text Entry #" + std::to_string(id) + ": " + v.Problem +
                           "; value retained (Copy/Select All available)");
        }
    }
}
size_t RuntimeUI::TextPayloadBytes(const Action &a) const
{
    return a.PreviousText.capacity() + a.TextValue.capacity();
}
size_t RuntimeUI::TextQueueBytes() const
{
    size_t bytes = 0;
    for (auto &a : m_Actions)
        if (a.Kind >= UIEventKind::TextChanged)
            bytes += TextPayloadBytes(a);
    return bytes;
}
size_t RuntimeUI::ReservedTextSlots() const
{
    if (!m_TextEdit.ID)
        return 0;
    bool preview = std::any_of(m_Actions.begin(), m_Actions.end(), [&](auto &a) {
        return a.Gesture == m_TextEdit.Terminal.Gesture && a.Kind == UIEventKind::TextChanged;
    });
    return preview ? 1 : 2;
}
bool RuntimeUI::BeginTextField(uint64_t id, bool pointer, glm::vec2 point)
{
    auto it = m_TextStates.find(id);
    if (it == m_TextStates.end() || !it->second.Valid || !Live(id))
        return false;
    if (m_TextEdit.ID && m_TextEdit.ID != id)
        EndTextField(false, false, true);
    auto &v = it->second;
    if (!m_TextEdit.ID)
    {
        Action terminal;
        terminal.Entity = terminal.Control = id;
        terminal.Kind = UIEventKind::TextCommitted;
        terminal.Name = m_Scene.GetEntityByUUID(id).GetComponent<UIButtonComponent>().Action;
        terminal.SourceLifetime = m_Scene.GetAuthoringLifetime(id);
        terminal.SourceComponent = m_Scene.GetAuthoringComponentVersion(id, typeid(UIButtonComponent));
        terminal.ControlComponent = m_Scene.GetAuthoringComponentVersion(id, typeid(UITextEntryComponent));
        if (!StampEvent(terminal) ||
            m_Actions.size() + ReservedSliderSlots() + ReservedDropdownSlots() + 2 >
                InputRouter::MaxActions ||
            TextQueueBytes() + 4 * Text::EditBuffer::MaxBytes + 256 > 128 * 1024)
        {
            ControlProblem("Text edit needs two event slots and bounded payload reserve; entry rejected");
            m_Input.CancelCapture();
            return false;
        }
        TextEdit edit;
        edit.ID = id;
        edit.Revision = v.Revision;
        edit.Before = v.Buffer;
        edit.Terminal = std::move(terminal);
        edit.Terminal.Gesture = ++m_NextGesture;
        edit.SuppressText = !pointer;
        m_TextEdit = std::move(edit);
        v.Buffer.BreakUndo();
    }
    PrepareTextField(id);
    m_TextEdit.Drag = pointer;
    m_KeyboardOwns = true;
    m_TipPointer = false;
    m_TextTime = 0;
    if (pointer)
    {
        auto n = m_Resolved.Find(id);
        auto ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
        auto content = n->Bounds.Inset(m_Scene.GetEntityByUUID(id).GetComponent<UIRectComponent>().Padding);
        auto local = point / (ratio * n->Scale) - content.Position;
        local = local / v.Size + v.Scroll;
        if (v.Layout)
            v.Buffer.Point(v.Geometry.Hit(local), m_TextShift);
        else
            v.Buffer.Apply({Text::EditCommand::All});
    }
    // Hit the input generation before reveal can translate its ancestor viewport.
    if (auto n = m_Resolved.Find(id); n && n->ScrollParent)
        EnsureVisible(0, id, false);
    RevealTextCaret(id);
    return true;
}
bool RuntimeUI::QueueTextChange(const Text::EditBuffer &next)
{
    auto &v = m_TextStates.at(m_TextEdit.ID);
    Action event = m_TextEdit.Terminal;
    event.Kind = UIEventKind::TextChanged;
    event.PreviousText = v.Buffer.Value();
    event.TextValue = next.Value();
    auto previous = std::find_if(m_Actions.begin(), m_Actions.end(), [&](auto &a) {
        return a.Gesture == event.Gesture && a.Kind == UIEventKind::TextChanged;
    });
    size_t bytes = TextQueueBytes();
    if (previous != m_Actions.end())
    {
        event.PreviousText = previous->PreviousText;
        bytes -= TextPayloadBytes(*previous);
    }
    if (bytes + TextPayloadBytes(event) + 2 * Text::EditBuffer::MaxBytes + 128 > 128 * 1024)
    {
        ControlProblem("Text event payload limit (128 KiB); edit rejected");
        return false;
    }
    if (!StampEvent(event) || !EventValid(event))
        return false;
    if (previous != m_Actions.end())
    {
        event.Sequence = previous->Sequence;
        *previous = std::move(event);
        return true;
    }
    if (m_Actions.size() + ReservedSliderSlots() + ReservedDropdownSlots() + 1 >= InputRouter::MaxActions)
    {
        ControlProblem("Text event admission failed; edit unchanged");
        return false;
    }
    event.Sequence = ++m_EventSequence;
    m_Actions.push_back(std::move(event));
    return true;
}
void RuntimeUI::EndTextField(bool cancel, bool interrupted, bool deliver)
{
    if (!m_TextEdit.ID)
        return;
    auto edit = std::move(m_TextEdit);
    m_TextEdit = {};
    auto it = m_TextStates.find(edit.ID);
    if (it == m_TextStates.end())
        return;
    auto &v = it->second;
    auto event = std::move(edit.Terminal);
    event.Kind = cancel ? UIEventKind::TextCancelled : UIEventKind::TextCommitted;
    if (interrupted)
        event.Kind = UIEventKind::TextCancelled;
    event.Interrupted = interrupted;
    event.PreviousText = edit.Before.Value();
    const bool restore = cancel && !interrupted && v.Revision == edit.Revision;
    event.TextValue = restore ? edit.Before.Value() : v.Buffer.Value();
    if (restore)
    {
        v.Buffer = std::move(edit.Before);
        ++v.Revision;
        v.Prepared = false;
    }
    v.Buffer.BreakUndo();
    if (m_Input.Captured() == edit.ID)
        m_Input.CancelCapture();
    if (deliver && EventValid(event))
    {
        event.Sequence = ++m_EventSequence;
        m_Actions.push_back(std::move(event));
    }
    m_TextRearm = true;
    m_ActionOwnsFrame = true;
    m_TextTime = 0;
}
void RuntimeUI::ReconcileTextField()
{
    if (!m_TextEdit.ID)
        return;
    auto it = m_TextStates.find(m_TextEdit.ID);
    if (it == m_TextStates.end() || !it->second.Valid || !Live(m_TextEdit.ID) ||
        !EventValid(m_TextEdit.Terminal))
    {
        EndTextField(false, true, false);
        return;
    }
    if (m_Input.Focused() != m_TextEdit.ID)
    {
        // A deliberate focus transfer retains content and completes the edit
        // while its source/scope/owner are still eligible for delivery.
        EndTextField(false, true, true);
        return;
    }
    PrepareTextField(m_TextEdit.ID);
}
void RuntimeUI::TextCommand(const Text::EditInput &input)
{
    if (!m_TextEdit.ID || !m_InputEnabled || m_TransitionNeutral)
        return;
    if (input.Command == Text::EditCommand::Insert && m_TextEdit.SuppressText)
        return;
    ReconcileTextField();
    if (!m_TextEdit.ID)
        return;
    auto id = m_TextEdit.ID;
    auto &v = m_TextStates.at(id);
    using C = Text::EditCommand;
    if (input.Command == C::Cancel)
    {
        EndTextField(true, false, true);
        return;
    }
    if (input.Command == C::Submit || (input.Command == C::Enter && !v.Config.Multiline))
    {
        EndTextField(false, false, true);
        return;
    }
    if (input.Command == C::Next || input.Command == C::Previous)
    {
        EndTextField(false, false, true);
        InputFrame navigation;
        navigation.Enabled = navigation.KeyboardEnabled = navigation.FocusNavigation = true;
        navigation.NextPressed = input.Command == C::Next;
        navigation.PreviousPressed = input.Command == C::Previous;
        NavigateScroll(navigation);
        m_Input.Update(m_Scene.GetIdentity(), m_Resolved.Buttons, navigation, m_Resolved.Generation);
        return;
    }
    try
    {
        if (!m_Clipboard)
            m_Clipboard = CreateRef<NativeTextClipboard>();
        const bool navigation = input.Command >= C::Left && input.Command <= C::All;
        if (navigation || input.Command == C::Copy)
        {
            v.Buffer.Apply(input, v.Layout ? &v.Geometry : nullptr, m_Clipboard.get());
        }
        else
        {
            auto candidate = v.Buffer;
            if (candidate.Apply(input, v.Layout ? &v.Geometry : nullptr, m_Clipboard.get()) &&
                QueueTextChange(candidate))
            {
                v.Buffer = std::move(candidate);
                ++v.Revision;
                m_TextEdit.Revision = v.Revision;
                v.Prepared = false;
                PrepareTextField(id);
            }
        }
        RevealTextCaret(id);
        m_TextTime = 0;
    }
    catch (const std::exception &error)
    {
        v.Problem = error.what();
        ControlProblem("Text Entry #" + std::to_string(id) + ": " + v.Problem);
    }
    m_ActionOwnsFrame = true;
}
void RuntimeUI::RouteTextControl(const Action &action)
{
    if (action.Phase == RoutedPhase::Activate)
    {
        if (m_TextEdit.ID == action.Entity)
            EndTextField(false);
        else
            BeginTextField(action.Entity, false);
        return;
    }
    if (action.Phase == RoutedPhase::Cancel)
    {
        EndTextField(true);
        return;
    }
    if (action.Phase == RoutedPhase::Begin)
    {
        BeginTextField(action.Entity, true, action.Point);
        return;
    }
    if (m_TextEdit.ID != action.Entity)
        return;
    auto &v = m_TextStates.at(action.Entity);
    if ((action.Phase == RoutedPhase::Move || action.Phase == RoutedPhase::End) && m_TextEdit.Drag)
    {
        PrepareTextField(action.Entity);
        auto n = m_Resolved.Find(action.Entity);
        auto ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
        auto content =
            n->Bounds.Inset(m_Scene.GetEntityByUUID(action.Entity).GetComponent<UIRectComponent>().Padding);
        auto point = (action.Point / (ratio * n->Scale) - content.Position) / v.Size + v.Scroll;
        if (v.Layout)
            v.Buffer.Point(v.Geometry.Hit(point), true);
        RevealTextCaret(action.Entity);
        m_TextTime = 0;
    }
    if (action.Phase == RoutedPhase::End)
        m_TextEdit.Drag = false;
}
bool RuntimeUI::RouteTextFrame(InputFrame &input)
{
    if(!input.Enabled){EndTextField(false,true,false);m_TextRearm=m_TextPointerNeutral=false;return false;}
    if (m_TextRearm)
    {
        m_ActionOwnsFrame = true;
        if (!input.LeftDown && !input.AcceptDown && !input.CancelDown && !input.TextKeysHeld)
        {
            m_TextRearm = false;
            InputFrame neutral;
            neutral.Enabled = true;
            m_Input.Update(m_Scene.GetIdentity(), m_Resolved.Buttons, neutral, m_Resolved.Generation);
        }
        return true;
    }
    if (m_TextPointerNeutral)
    {
        m_ActionOwnsFrame = true;
        if (!input.LeftDown)
            m_TextPointerNeutral = false;
        return true;
    }
    if (!m_TextEdit.ID)
        return false;
    if (!input.AcceptDown && !input.TextKeysHeld)
        m_TextEdit.SuppressText = false;
    if (!input.Enabled)
    {
        EndTextField(false, true, false);
        return false;
    }
    ReconcileTextField();
    if (!m_TextEdit.ID)
        return false;
    if (input.PointerPressed)
    {
        auto n = m_Resolved.Find(m_TextEdit.ID);
        if (!n || !n->FramebufferBounds.Intersect(n->FramebufferClip).Contains(input.Mouse))
        {
            EndTextField(false, false, true);
            m_TextPointerNeutral = true;
            m_Input.CancelCapture();
            m_ActionOwnsFrame = true;
            return true;
        }
    }
    if (input.CancelDown)
    {
        EndTextField(true);
        m_ActionOwnsFrame = true;
        return true;
    }
    if (input.AcceptPressed)
    {
        auto &v = m_TextStates.at(m_TextEdit.ID);
        if (v.Config.Multiline)
            TextCommand({Text::EditCommand::Submit});
        else
            EndTextField(false);
        return true;
    }
    if (input.Scroll.x || input.Scroll.y)
    {
        auto &v = m_TextStates.at(m_TextEdit.ID);
        if (v.Layout)
        {
            auto n = m_Resolved.Find(m_TextEdit.ID);
            auto size =
                n->Bounds
                    .Inset(m_Scene.GetEntityByUUID(m_TextEdit.ID).GetComponent<UIRectComponent>().Padding)
                    .Size /
                v.Size;
            auto extent = glm::vec2(v.Layout->Width, v.Layout->Height);
            auto delta = v.Config.Multiline ? input.Scroll : glm::vec2(input.Scroll.x + input.Scroll.y, 0);
            v.Scroll =
                glm::clamp(v.Scroll + delta * 3.f, glm::vec2(0), glm::max(glm::vec2(0), extent - size));
        }
        input.Scroll = {};
    }
    input.Navigate = {};
    input.Adjust = 0;
    input.NextPressed = input.PreviousPressed = input.FocusNavigation = false;
    input.NextDown = input.PreviousDown = false;
    input.Editing = false; // Text Cancel is handled above, not slider Cancel.
    // Pointer capture still runs through the one InputRouter.
    return false;
}
void RuntimeUI::RevealTextCaret(uint64_t id)
{
    auto &v = m_TextStates.at(id);
    if (!v.Layout || v.Size <= 0)
        return;
    auto n = m_Resolved.Find(id);
    auto content = n->Bounds.Inset(m_Scene.GetEntityByUUID(id).GetComponent<UIRectComponent>().Padding);
    auto visible = content.Size / v.Size;
    auto caret = v.Geometry.Locate(v.Buffer.Selection().Caret);
    auto target = caret.Point;

    if (target.x < v.Scroll.x)
        v.Scroll.x = target.x;
    if (target.x + .08f > v.Scroll.x + visible.x)
        v.Scroll.x = target.x + .08f - visible.x;
    if (target.y < v.Scroll.y)
        v.Scroll.y = target.y;
    if (target.y + caret.Height > v.Scroll.y + visible.y)
        v.Scroll.y = target.y + caret.Height - visible.y;
    v.Scroll = glm::max(v.Scroll, glm::vec2(0));
}
void RuntimeUI::DrawTextField(const ResolvedElement &n, Entity entity, const RasterState &raster)
{
    auto it = m_TextStates.find(n.ID);
    if (it == m_TextStates.end())
        return;
    PrepareTextField(n.ID);
    auto &v = it->second;
    auto content = n.Bounds.Inset(entity.GetComponent<UIRectComponent>().Padding);
    auto logical = n.Clip.Intersect(content);
    auto ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
    auto coverage = FramebufferCoverage({logical.Position * n.Scale, logical.Size * n.Scale}, ratio,
                                        m_Resolved.Surface.Framebuffer);
    Renderer2D::SetPrimitiveClip(
        {raster.Viewport.x + int(coverage.Position.x),
         raster.Viewport.y + raster.Viewport.w - int(coverage.Position.y + coverage.Size.y),
         int(coverage.Size.x), int(coverage.Size.y)});
    auto quad = [&](glm::vec2 pos, glm::vec2 size, glm::vec4 color) {
        if (size.x <= 0 || size.y <= 0)
            return;
        auto center = (content.Position + (pos - v.Scroll) * v.Size + size * v.Size * .5f) * n.Scale;
        auto transform = glm::translate(glm::mat4(1), glm::vec3(center.x, -center.y, 0)) *
                         glm::scale(glm::mat4(1), glm::vec3(size * v.Size * n.Scale, 1));
        Renderer2D::DrawQuad(transform, color, int(uint32_t(entity)));
    };
    if (!v.Layout || !v.Paint)
    {
        quad({}, {.2f, .8f}, {1, .3f, .1f, 1});
        return;
    }
    if (m_TextEdit.ID == n.ID)
    {
        for (auto box :
             v.Geometry.Selection(v.Buffer.Selection().Anchor.Scalar, v.Buffer.Selection().Caret.Scalar))
            quad(box.Position, box.Size, v.Config.SelectionColor);
    }
    auto &cache = m_Cache[n.ID];
    const auto bytesBefore = cache.Glyphs.capacity() * sizeof(Renderer2D::TextGlyph);
    bool accounted = false;
    try
    {
        if (!cache.PreparedValid && !v.PaintFailed)
        {
            if (m_PreparedBytes - bytesBefore + v.Paint->Glyphs.size() * sizeof(Renderer2D::TextGlyph) >
                16 * 1024 * 1024)
                throw Text::ResourcePressure("UI prepared glyph budget exceeded; text value retained");
            cache.Glyphs.clear();
            cache.Glyphs.reserve(v.Paint->Glyphs.size());
            for (uint32_t i = 0; i < v.Paint->Lines.size(); ++i)
            {
                Renderer2D::TextLine line{"", v.Paint->Lines[i].Width, v.Paint, i};
                Renderer2D::AppendTextLineGlyphs(line, v.FontAsset, cache.Glyphs,
                                                 {0, -v.Paint->Lines[i].Baseline});
            }
            m_PreparedBytes =
                m_PreparedBytes - bytesBefore + cache.Glyphs.capacity() * sizeof(Renderer2D::TextGlyph);
            accounted = true;
            cache.PreparedValid = true;
        if(!v.FontAsset->PageError().empty())v.Problem=v.FontAsset->PageError();
            ++m_Diagnostics.PreparedRebuilds;
        }
        else if (cache.PreparedValid)
            ++m_Diagnostics.PreparedReuses;
        if (cache.PreparedValid)
        {
            auto origin = (content.Position - v.Scroll * v.Size) * n.Scale;
            auto transform = glm::translate(glm::mat4(1), glm::vec3(origin.x, -origin.y, 0)) *
                             glm::scale(glm::mat4(1), glm::vec3(v.Size * n.Scale));
            auto color = entity.GetComponent<TextComponent>().Color;
            if (v.Buffer.Value().empty())
                color.a *= .6f;
            Renderer2D::DrawTextGlyphs(cache.Glyphs, v.FontAsset, transform, color, int(uint32_t(entity)));
            cache.PaintLayout = v.Layout;
            cache.PaintGeneration = m_Resolved.Generation;
        }
    }
    catch (const std::exception &error)
    {
        if (!accounted)
            m_PreparedBytes =
                m_PreparedBytes - bytesBefore + cache.Glyphs.capacity() * sizeof(Renderer2D::TextGlyph);
        cache.Glyphs.clear();
        cache.PreparedValid = false;
        v.PaintFailed = true;
        v.Problem = error.what();
        ControlProblem("Text Entry #" + std::to_string(n.ID) + ": " + v.Problem);
    }
    if (m_TextEdit.ID == n.ID && m_TextTime < .55f)
    {
        auto caret = v.Geometry.Locate(v.Buffer.Selection().Caret);
        quad(caret.Point, {std::max(.04f, 1 / (v.Size * n.Scale)), caret.Height}, v.Config.CaretColor);
    }
    m_Diagnostics.PreparedBytes = m_PreparedBytes;
}
} // namespace Hazel
