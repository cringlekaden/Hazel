#include "hzpch.h"
#include "UIStyleApplication.h"
namespace Hazel
{
void ApplyUIStyle(UIAppearanceComponent &c, const UIStyleValues &v)
{
    c.Focus = v.Focus;
    c.Selected = v.Selected;
    c.Editing = v.Editing;
}
void ApplyUIStyle(UIRectComponent &c, const UIStyleValues &v)
{
    c.Padding = v.Padding;
}
void ApplyUIStyle(UITextStyleComponent &c, const UIStyleValues &v)
{
    c.Size = v.Size;
}
void ApplyUIStyle(TextComponent &c, const UIStyleValues &v)
{
    c.Font = v.Font;
    c.Color = v.Foreground;
    c.Kerning = v.Spacing.x;
    c.LineSpacing = v.Spacing.y;
}
void ApplyUIStyle(SpriteRendererComponent &c, const UIStyleValues &v)
{
    c.Color = v.Background;
    c.Source = v.Image;
}
void ApplyUIStyle(UIButtonComponent &c, const UIStyleValues &v)
{
    c.Colors = v.States.Colors;
    c.Images = v.States.Images;
}
void ApplyUIStyle(UITextEntryComponent &c, const UIStyleValues &v)
{
    c.CaretColor = v.Caret;
    c.SelectionColor = v.Selection;
}
void ApplyUIStyle(UINineSliceComponent &c, const UIStyleValues &v)
{
    c.Borders = v.Slice.Borders;
    c.BorderScale = v.Slice.Scale;
}
void ExtractUIStyle(const UIAppearanceComponent &c, UIStyleValues &v)
{
    v.Focus = c.Focus;
    v.Selected = c.Selected;
    v.Editing = c.Editing;
}
void ExtractUIStyle(const UIRectComponent &c, UIStyleValues &v)
{
    v.Padding = c.Padding;
}
void ExtractUIStyle(const UITextStyleComponent &c, UIStyleValues &v)
{
    v.Size = c.Size;
}
void ExtractUIStyle(const TextComponent &c, UIStyleValues &v)
{
    v.Font = c.Font;
    v.Foreground = c.Color;
    v.Spacing = {c.Kerning, c.LineSpacing};
}
void ExtractUIStyle(const SpriteRendererComponent &c, UIStyleValues &v)
{
    v.Background = c.Color;
    v.Image = c.Source;
}
void ExtractUIStyle(const UIButtonComponent &c, UIStyleValues &v)
{
    v.States = {c.Colors, c.Images};
}
void ExtractUIStyle(const UITextEntryComponent &c, UIStyleValues &v)
{
    v.Caret = c.CaretColor;
    v.Selection = c.SelectionColor;
}
void ExtractUIStyle(const UINineSliceComponent &c, UIStyleValues &v)
{
    v.Slice = {c.Borders, c.BorderScale};
}
UIStyleValues LocalUIStyle(Entity e)
{
    UIStyleValues v;
#define COMPONENT(T)                                                                               \
    if (e.HasComponent<T>())                                                                       \
        ExtractUIStyle(e.GetComponent<T>(), v);
    COMPONENT(UIRectComponent)
    COMPONENT(UITextStyleComponent) COMPONENT(TextComponent) COMPONENT(SpriteRendererComponent)
        COMPONENT(UIButtonComponent) COMPONENT(UITextEntryComponent) COMPONENT(UINineSliceComponent)
#undef COMPONENT
            if (e.HasComponent<UIAppearanceComponent>())
    {
        auto &c = e.GetComponent<UIAppearanceComponent>();
        v.Focus = c.Focus;
        v.Selected = c.Selected;
        v.Editing = c.Editing;
    }
    return v;
}
UIStyleValues ResolveUIStyle(Entity e, const Ref<ProjectAssets> &assets,
                             Ref<const UIStyleDefinition> &accepted, std::string &error)
{
    auto local = LocalUIStyle(e);
    if (!e.HasComponent<UIAppearanceComponent>() || !e.GetComponent<UIAppearanceComponent>().Styled)
        return local;
    auto &c = e.GetComponent<UIAppearanceComponent>();
    c.Selection.Validate();
    if (assets)
    {
        auto result = assets->Style(c.Selection.Asset);
        accepted = result.Accepted;
        error = result.Error;
    }
    else if (c.Selection.Asset.empty())
    {
        static const Ref<const UIStyleDefinition> engine(&DefaultUIStyle(), [](const UIStyleDefinition*) {});
        accepted = engine;
    }
    else
        error = "Project style needs project assets";
    UIStyleValues resolved = accepted ? accepted->Find(c.Selection.Variant) : UIStyleValues{};
    CopyUIStyleGroups(resolved, local, c.Overrides);
    resolved.Validate();
    return resolved;
}
} // namespace Hazel
