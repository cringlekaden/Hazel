#include "UIEditCommand.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/UTF8.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace Hazel
{
namespace
{
void Strip(SpriteSource &source)
{
    if (auto t = std::get_if<TextureSpriteSource>(&source))
        t->Resource.reset();
}
template <class T> bool Equal(const T &a, const T &b)
{
    return a == b;
}
bool Equal(float a, float b)
{
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}
template <glm::length_t L, class T, glm::qualifier Q>
bool Equal(const glm::vec<L, T, Q> &a, const glm::vec<L, T, Q> &b)
{
    for (glm::length_t i = 0; i < L; ++i)
        if (!Equal(a[i], b[i]))
            return false;
    return true;
}
bool Equal(const GameUI::Track &a, const GameUI::Track &b)
{ return a.Mode==b.Mode && Equal(a.Value,b.Value) && Equal(a.Minimum,b.Minimum) && Equal(a.Maximum,b.Maximum) && a.HasMaximum==b.HasMaximum; }
bool Equal(const std::vector<GameUI::Track> &a,const std::vector<GameUI::Track> &b)
{ if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(!Equal(a[i],b[i]))return false;return true; }
bool Equal(const SpriteSource &a, const SpriteSource &b)
{
    if (a.index() != b.index())
        return false;
    if (auto t = std::get_if<TextureSpriteSource>(&a))
    {
        auto &v = std::get<TextureSpriteSource>(b);
        return t->Texture == v.Texture && Equal(t->TilingFactor, v.TilingFactor);
    }
    if (auto r = std::get_if<SpriteReference>(&a))
        return *r == std::get<SpriteReference>(b);
    return true;
}

// One field enumeration supplies exact comparison, merge and byte accounting.
// Presence is not edited by this milestone; component retirement rejects commands.
template <class A, class B, class C, class F> void Fields(A &a, B &b, C &c, F f)
{
#define COMPONENT(member)                                                                          \
    if (bool(a.member) != bool(b.member) || bool(a.member) != bool(c.member))                      \
        throw std::runtime_error("UI component set changed; cancel/retry the edit");               \
    if (a.member)
#define FIELD(member, field) f(a.member->field, b.member->field, c.member->field)
    COMPONENT(Canvas)
    {
        FIELD(Canvas, ReferenceSize);
        FIELD(Canvas, SafeInsets);
        FIELD(Canvas, SortOrder);
        FIELD(Canvas, Visible);
        FIELD(Canvas, Enabled);
        FIELD(Canvas, Modal);
    }
    COMPONENT(Rect)
    {
        FIELD(Rect, WidthMode);
        FIELD(Rect, HeightMode);
        FIELD(Rect, FixedSize);
        FIELD(Rect, MaximumSize);
        FIELD(Rect, HasMaximum);
        FIELD(Rect, Anchor);
        FIELD(Rect, Offset);
        FIELD(Rect, Pivot);
        FIELD(Rect, MinimumSize);
        FIELD(Rect, Margin);
        FIELD(Rect, Padding);
        FIELD(Rect, Flex);
        FIELD(Rect, Visible);
        FIELD(Rect, Enabled);
        FIELD(Rect, ClipChildren);
        FIELD(Rect, GridRow);
        FIELD(Rect, GridColumn);
        FIELD(Rect, RowSpan);
        FIELD(Rect, ColumnSpan);
        FIELD(Rect, Fit);
        FIELD(Rect, ImageAlignment);
    }
    COMPONENT(Flow)
    {
        FIELD(Flow, Direction);
        FIELD(Flow, Main);
        FIELD(Flow, Cross);
        FIELD(Flow, Gap);
FIELD(Flow, Kind);
FIELD(Flow, LineGap);
FIELD(Flow, LineAlign);
FIELD(Flow, GridGap);
FIELD(Flow, Columns);
FIELD(Flow, Rows);

    }
    COMPONENT(Style)
    {
        FIELD(Style, Size);
        FIELD(Style, Horizontal);
        FIELD(Style, Vertical);
        FIELD(Style, Wrap);
        FIELD(Style, Overflow);
        FIELD(Style, MaxLines);
    }
    COMPONENT(Slice)
    {
        FIELD(Slice, Borders);
        FIELD(Slice, BorderScale);
    }
    COMPONENT(Button)
    {
        FIELD(Button, Action);
        FIELD(Button, TabOrder);
        FIELD(Button, Interactable);
        for (int i = 0; i < 5; ++i)
        {
            f(a.Button->Colors[i], b.Button->Colors[i], c.Button->Colors[i]);
            f(a.Button->Images[i], b.Button->Images[i], c.Button->Images[i]);
        }
    }
    COMPONENT(Text)
    {
        FIELD(Text, Text);
        FIELD(Text, Font);
        FIELD(Text, Color);
        FIELD(Text, Kerning);
        FIELD(Text, LineSpacing);
    }
    COMPONENT(Image)
    {
        FIELD(Image, Color);
        FIELD(Image, Source);
    }
#undef COMPONENT
#undef FIELD
}
template <class T> size_t Storage(const T &)
{
    return 0;
}
size_t Storage(const std::vector<GameUI::Track> &v) { return v.capacity()*sizeof(GameUI::Track); }
size_t Storage(const std::string &v)
{
    return v.capacity() + 1;
}
size_t Storage(const std::filesystem::path &v)
{
    return (v.native().capacity() + 1) * sizeof(std::filesystem::path::value_type);
}
size_t Storage(const FontSelection &v) {return Storage(v.Family);}
size_t Storage(const SpriteSource &v)
{
    if (auto t = std::get_if<TextureSpriteSource>(&v))
        return Storage(t->Texture);
    if (auto r = std::get_if<SpriteReference>(&v))
        return Storage(r->Sheet);
    return 0;
}
template <class T> void Finite(const T &v)
{
    for (int i = 0; i < T::length(); ++i)
        if (!std::isfinite(v[i]))
            throw std::invalid_argument("UI color/border must be finite");
}
void Positive(float v, bool zero = true)
{
    if (!std::isfinite(v) || v < 0 || (!zero && v == 0))
        throw std::invalid_argument("UI dimension must be finite and positive");
}
bool Unassigned(const SpriteSource &source)
{
    if (auto t = std::get_if<TextureSpriteSource>(&source)) return t->Texture.empty();
    if (auto r = std::get_if<SpriteReference>(&source)) return r->Sheet.empty() && !r->Region;
    return source.index() == 0;
}
bool Generated(const SpriteSource &source)
{
    auto t = std::get_if<TextureSpriteSource>(&source);
    return t && t->Texture.empty() && bool(t->Resource);
}
void Source(Scene &scene, const SpriteSource &source)
{
    if (auto t = std::get_if<TextureSpriteSource>(&source)) Positive(t->TilingFactor);
    if (Unassigned(source))
        return;
    auto assets = scene.GetAssets();
    if (!assets)
        throw std::invalid_argument("Open a project before assigning an image");
    auto resolved = assets->Resolve(source);
    if (!resolved)
        throw std::invalid_argument("Image asset could not be resolved");
}
void Validate(Scene &scene, const UIEditValues &v, const UIEditValues &old)
{
    if (v.Canvas)
        GameUI::FitCanvas({1280, 720}, *v.Canvas);
    if (v.Rect)
    {
        GameUI::Validate(*v.Rect);
        Positive(v.Rect->Flex);
    }
    if(v.Rect){
        GameUI::FitImage({}, {1,1},v.Rect->Fit,v.Rect->ImageAlignment);
        if(v.Slice&&v.Rect->Fit!=GameUI::ImageFit::Stretch)throw std::invalid_argument("Nine-slice requires Stretch image fit");
    }
    if (v.Flow)
    {
        GameUI::Validate(*v.Flow);
    }
    if (v.Style)
    {
        auto &s = *v.Style;
        Positive(s.Size, false);
        if (s.Size > 512 || int(s.Horizontal) < 0 || int(s.Horizontal) > 2 || int(s.Vertical) < 0 ||
            int(s.Vertical) > 2 || unsigned(s.Overflow)>1 || s.MaxLines>4096)
            throw std::invalid_argument("Invalid text style size/alignment");

    }
    if (v.Slice)
    {
        Finite(v.Slice->Borders);
        for (int i = 0; i < 4; ++i)
            Positive(v.Slice->Borders[i]);
        Positive(v.Slice->BorderScale, false);
    }
    if (v.Button)
    {
        if (v.Button->Action.size() > 128 || v.Button->Action.find('\0') != std::string::npos)
            throw std::invalid_argument("Action identity allows 128 bytes and no NUL");
        for (int i = 0; i < 5; ++i)
        {
            Finite(v.Button->Colors[i]);
            if (!old.Button || !(v.Button->Images[i] == old.Button->Images[i]))
                Source(scene, v.Button->Images[i]);
        }
    }
    if (v.Text)
    {
        if (v.Text->Text.size() > 65536)
            throw std::invalid_argument("UI text exceeds 64 KiB");
        UTF8::Validate(v.Text->Text);v.Text->Font.Validate();
        if(!v.Text->Font.Family.empty()) {
            auto assets=scene.GetAssets();if(!assets)throw std::runtime_error("Text family needs project assets");
            (void)Project::ResolveOwnedAsset(assets->Root(),v.Text->Font.Family);
        }
        Finite(v.Text->Color);
        if (!std::isfinite(v.Text->Kerning) || !std::isfinite(v.Text->LineSpacing))
            throw std::invalid_argument("Text spacing must be finite");
    }
    if (v.Image)
    {
        Finite(v.Image->Color);
        if (!old.Image || !(v.Image->Source == old.Image->Source))
            Source(scene, v.Image->Source);
    }
    if (v.Slice && v.Image)
    {
        bool changed = !old.Slice || v.Slice->Borders != old.Slice->Borders ||
                       v.Slice->BorderScale != old.Slice->BorderScale || !old.Image ||
                       !(v.Image->Source == old.Image->Source);
        if (v.Button && old.Button)
            for (int i = 0; i < 5; ++i)
                changed |= !(v.Button->Images[i] == old.Button->Images[i]);
        if (changed)
        {
            auto check = [&](const SpriteSource &source) {
                if (Unassigned(source))
                    return; // unassigned assets remain editable authored values
                auto assets = scene.GetAssets();
                if (!assets)
                    throw std::invalid_argument(
                        "Open a project before assigning a nine-slice image");
                auto sprite = assets->Resolve(source);
                if (!sprite || !sprite->Texture)
                    throw std::invalid_argument("Nine-slice requires an image");
                auto size = sprite->PixelSize();
                if (v.Slice->Borders.x + v.Slice->Borders.z > size.x ||
                    v.Slice->Borders.y + v.Slice->Borders.w > size.y)
                    throw std::invalid_argument("Nine-slice borders exceed the source region");
            };
            check(v.Image->Source);
            if (v.Button)
                for (auto &source : v.Button->Images)
                    if (source.index())
                        check(source);
        }
    }
}
// Swap only changed authored fields. Prepared assets, procedural resources,
// runtime state and unrelated native writes remain untouched.
void Write(Entity e, const UIEditValues &old, UIEditValues &next)
{
    auto change = [](auto &live, const auto &a, auto &b) {
        if (!Equal(a, b))
        {
            using std::swap;
            swap(live, b);
        }
    };
#define COMPONENT(member, type)                                                                    \
    if (old.member && next.member)                                                                 \
    {                                                                                              \
        auto &c = e.GetComponent<type>();
#define FIELD(member, field) change(c.field, old.member->field, next.member->field)
    COMPONENT(Canvas, UICanvasComponent) FIELD(Canvas, ReferenceSize);
    FIELD(Canvas, SafeInsets);
    FIELD(Canvas, SortOrder);
    FIELD(Canvas, Visible);
    FIELD(Canvas, Enabled);
    FIELD(Canvas, Modal);
}
COMPONENT(Rect, UIRectComponent) FIELD(Rect, WidthMode);
FIELD(Rect, HeightMode);
FIELD(Rect, FixedSize);
FIELD(Rect, MaximumSize);
FIELD(Rect, HasMaximum);
FIELD(Rect, Anchor);
FIELD(Rect, Offset);
FIELD(Rect, Pivot);
FIELD(Rect, MinimumSize);
FIELD(Rect, Margin);
FIELD(Rect, Padding);
FIELD(Rect, Flex);
FIELD(Rect, Visible);
FIELD(Rect, Enabled);
FIELD(Rect, ClipChildren);
FIELD(Rect, GridRow);
FIELD(Rect, GridColumn);
FIELD(Rect, RowSpan);
FIELD(Rect, ColumnSpan);
FIELD(Rect, Fit);
FIELD(Rect, ImageAlignment);

}
COMPONENT(Flow, UILayoutComponent) FIELD(Flow, Direction);
FIELD(Flow, Main);
FIELD(Flow, Cross);
FIELD(Flow, Gap);
FIELD(Flow, Kind);
FIELD(Flow, LineGap);
FIELD(Flow, LineAlign);
FIELD(Flow, GridGap);
FIELD(Flow, Columns);
FIELD(Flow, Rows);

}
COMPONENT(Style, UITextStyleComponent)
FIELD(Style, Size);
FIELD(Style, Horizontal);
FIELD(Style, Vertical);
FIELD(Style, Wrap);
        FIELD(Style, Overflow);
        FIELD(Style, MaxLines);
}
COMPONENT(Slice, UINineSliceComponent) FIELD(Slice, Borders);
FIELD(Slice, BorderScale);
}
COMPONENT(Button, UIButtonComponent) FIELD(Button, Action);
FIELD(Button, TabOrder);
FIELD(Button, Interactable);
for (int i = 0; i < 5; ++i)
{
    change(c.Colors[i], old.Button->Colors[i], next.Button->Colors[i]);
    change(c.Images[i], old.Button->Images[i], next.Button->Images[i]);
}
}
COMPONENT(Text, TextComponent) change(c.TextString, old.Text->Text, next.Text->Text);
FIELD(Text, Font);
FIELD(Text, Color);
FIELD(Text, Kerning);
FIELD(Text, LineSpacing);
}
COMPONENT(Image, SpriteRendererComponent) FIELD(Image, Color);
FIELD(Image, Source);
}
#undef COMPONENT
#undef FIELD
}
UIEditValues Merge(const UIEditValues &start, const UIEditValues &end, const UIEditValues &current,
                   bool undo, bool cancel = false)
{
    auto next = current;
    Fields(start, end, next, [&](const auto &a, const auto &b, auto &value) {
        if (Equal(a, b))
            return;
        const auto &expected = undo ? b : a;
        if (!Equal(value, expected))
        {
            if (cancel)
                return;
            throw std::runtime_error("Authored field changed outside history; revert that change "
                                     "or clear this document's history");
        }
        value = undo ? a : b;
    });
    return next;
}
} // namespace
UIEditValues UIEditValues::Capture(Entity e)
{
    UIEditValues v;
#define CAPTURE(type, member)                                                                      \
    if (e.HasComponent<type>())                                                                    \
    v.Set(e.GetComponent<type>())
    CAPTURE(UICanvasComponent, Canvas);
    CAPTURE(UIRectComponent, Rect);
    CAPTURE(UILayoutComponent, Flow);
    CAPTURE(UITextStyleComponent, Style);
    CAPTURE(UINineSliceComponent, Slice);
    CAPTURE(UIButtonComponent, Button);
    CAPTURE(TextComponent, Text);
    CAPTURE(SpriteRendererComponent, Image);
#undef CAPTURE
    return v;
}
void UIEditValues::StripResources()
{
    if (Image)
        Strip(Image->Source);
    if (Button)
        for (auto &source : Button->Images)
            Strip(source);
}
size_t UIEditValues::Bytes() const
{
    size_t n = sizeof(*this);
    Fields(*this, *this, *this,
           [&](const auto &v, const auto &, const auto &) { n += Storage(v); });
    return n;
}
bool UIEditValues::Same(const UIEditValues &other) const
{
    bool same = true;
    try
    {
        Fields(*this, other, other,
               [&](const auto &a, const auto &b, const auto &) { same &= Equal(a, b); });
    }
    catch (const std::exception &)
    {
        return false;
    }
    return same;
}
UIEditCommand::UIEditCommand(Scene &scene, uint64_t entity, std::string name, UIEditValues before)
    : m_Document(scene.GetIdentity()), m_Entity(entity), m_Name(std::move(name)),
      m_Before(std::move(before)), m_After(m_Before)
{
    m_Before.StripResources();
    m_After.StripResources();
    auto e = scene.GetEntityByUUID(entity);
    m_ComponentVersions = {
        scene.GetAuthoringComponentVersion(entity, typeid(UICanvasComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(UIRectComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(UILayoutComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(UITextStyleComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(UINineSliceComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(UIButtonComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(TextComponent)),
        scene.GetAuthoringComponentVersion(entity, typeid(SpriteRendererComponent))};
    while (e)
    {
        if (m_Guards.size() >= Scene::MaxDepth)
            throw std::runtime_error("UI authoring ancestry exceeds depth limit");
        m_Guards.emplace_back(e.GetUUID(), m_Guards.empty()
                                               ? scene.GetAuthoringLifetime(e.GetUUID())
                                               : scene.GetAuthoringStructureVersion(e.GetUUID()));
        e = scene.GetEntityByUUID(scene.GetRelationship(e).Parent);
    }
    if (m_Guards.empty())
        throw std::runtime_error("Select a live UI entity");
}
Entity UIEditCommand::Target(Scene &scene, bool cancelling) const
{
    if (scene.GetIdentity() != m_Document)
        throw std::runtime_error("UI edit belongs to another document");
    if (scene.IsRunning() || scene.IsSimulating())
        throw std::runtime_error("Stop Play / Simulate before editing");
    auto e = scene.GetEntityByUUID(m_Entity), ancestor = e;
    if (!e || scene.GetAuthoringLifetime(m_Entity) != m_Guards.front().second)
        throw std::runtime_error("UI entity lifetime ended; select a live entity");
    if (cancelling)
        return e;
    const std::array<std::type_index, 8> types = {
        typeid(UICanvasComponent),    typeid(UIRectComponent),        typeid(UILayoutComponent),
        typeid(UITextStyleComponent), typeid(UINineSliceComponent),   typeid(UIButtonComponent),
        typeid(TextComponent),        typeid(SpriteRendererComponent)};
    for (size_t i = 0; i < types.size(); ++i)
        if (scene.GetAuthoringComponentVersion(m_Entity, types[i]) != m_ComponentVersions[i])
            throw std::runtime_error(
                "UI component was removed/replaced; clear obsolete history and retry");
    for (auto [id, version] : m_Guards)
    {
        if (!ancestor || uint64_t(ancestor.GetUUID()) != id || !version ||
            (id == m_Entity ? scene.GetAuthoringLifetime(id)
                            : scene.GetAuthoringStructureVersion(id)) != version)
            throw std::runtime_error("UI entity/component or ancestor was retired/reparented; "
                                     "clear obsolete history and retry");
        ancestor = scene.GetEntityByUUID(scene.GetRelationship(ancestor).Parent);
    }
    if (ancestor)
        throw std::runtime_error("UI ancestor chain changed; retry the edit");
    return e;
}
void UIEditCommand::Check(Scene &scene) const
{
    auto current = UIEditValues::Capture(Target(scene));
    // Commit/previews may coexist with unrelated native field changes.
    (void)Merge(m_Before, m_After, current, true);
}
size_t UIEditCommand::Bytes() const
{
    return sizeof(*this) + m_Name.capacity() + 1 + m_Guards.capacity() * sizeof(m_Guards[0]) +
           m_Before.Bytes() + m_After.Bytes();
}
void UIEditCommand::Publish(Scene &scene, const UIEditValues &current, UIEditValues next,
                            bool cancelling) const
{
    auto e = Target(scene, cancelling);
    auto preserveGenerated = [](const SpriteSource &live, const SpriteSource &a, const SpriteSource &b) {
        if (!Equal(a, b) && Generated(live))
            throw std::invalid_argument("Generated native images cannot enter history; assign a file asset natively first");
    };
    if (current.Image && next.Image)
        preserveGenerated(e.GetComponent<SpriteRendererComponent>().Source, current.Image->Source, next.Image->Source);
    if (current.Button && next.Button)
        for (int i = 0; i < 5; ++i)
            preserveGenerated(e.GetComponent<UIButtonComponent>().Images[i], current.Button->Images[i], next.Button->Images[i]);
    Validate(scene, next, current);
    auto restore = next;     // all allocations precede mutation
    Write(e, current, next); // next now contains the old values for changed fields
    try
    {
        scene.ValidateUILayoutDependencies();
        scene.RefreshUI();
    }
    catch (...)
    {
        Write(e, restore, next);
        try
        {
            scene.RefreshUI();
        }
        catch (...)
        {
        }
        throw;
    }
}
bool UIEditCommand::Preview(Scene &scene, UIEditValues proposed, std::string &error)
{
    return Preview(scene, m_After, std::move(proposed), error);
}
bool UIEditCommand::Preview(Scene &scene, const UIEditValues &sampleBefore, UIEditValues proposed,
                            std::string &error)
{
    try
    {
        proposed.StripResources();
        Check(scene);
        auto current = UIEditValues::Capture(Target(scene));
        // Preserve fields changed elsewhere since the last sample, unless this
        // sample explicitly edits that same field (then reject rather than overwrite).
        auto next = Merge(sampleBefore, proposed, current, false);
        auto baseline = m_Before, retained = m_After;
        // A field joins the transaction when first edited, not merely when
        // captured. Preserve a native value introduced before we own that field.
        Fields(baseline, retained, current, [](auto &a, auto &b, const auto &value) {
            if (Equal(a, b))
                a = b = value;
        });
        Fields(sampleBefore, proposed, retained, [](const auto &a, const auto &b, auto &v) {
            if (!Equal(a, b))
                v = b;
        });
        if (sizeof(*this) + m_Name.capacity() + 1 + m_Guards.capacity() * sizeof(m_Guards[0]) +
                baseline.Bytes() + retained.Bytes() >
            m_MemoryLimit)
            throw std::runtime_error(
                "Edit exceeds history memory limit; shorten the value or cancel");
        Publish(scene, current, std::move(next));
        m_Before = std::move(baseline);
        m_After = std::move(retained);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
void UIEditCommand::Apply(Scene &scene, bool undo)
{
    auto current = UIEditValues::Capture(Target(scene));
    Publish(scene, current, Merge(m_Before, m_After, current, undo));
}
void UIEditCommand::Cancel(Scene &scene)
{
    auto current = UIEditValues::Capture(Target(scene, true)), before = m_Before, after = m_After,
         subset = current;
    // Restore only unchanged-lifetime components. Native retirement/replacement
    // takes precedence; unrelated fields and newly authored components survive.
#define KEEP(member, type, index)                                                                  \
    if (!current.member ||                                                                         \
        scene.GetAuthoringComponentVersion(m_Entity, typeid(type)) != m_ComponentVersions[index])  \
    {                                                                                              \
        before.member.reset();                                                                     \
        after.member.reset();                                                                      \
        subset.member.reset();                                                                     \
    }
    KEEP(Canvas, UICanvasComponent, 0);
    KEEP(Rect, UIRectComponent, 1);
    KEEP(Flow, UILayoutComponent, 2);
    KEEP(Style, UITextStyleComponent, 3);
    KEEP(Slice, UINineSliceComponent, 4);
    KEEP(Button, UIButtonComponent, 5);
    KEEP(Text, TextComponent, 6);
    KEEP(Image, SpriteRendererComponent, 7);
#undef KEEP
    auto restored = Merge(before, after, subset, true, true), next = current;
#define RESTORE(member)                                                                            \
    if (restored.member)                                                                           \
    next.member = std::move(restored.member)
    RESTORE(Canvas);
    RESTORE(Rect);
    RESTORE(Flow);
    RESTORE(Style);
    RESTORE(Slice);
    RESTORE(Button);
    RESTORE(Text);
    RESTORE(Image);
#undef RESTORE
    Publish(scene, current, std::move(next), true);
}
} // namespace Hazel
