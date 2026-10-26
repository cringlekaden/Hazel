#pragma once
#include "Hazel/Scene/Entity.h"
namespace Hazel
{
UIStyleValues LocalUIStyle(Entity entity);
UIStyleValues ResolveUIStyle(Entity entity, const Ref<ProjectAssets> &assets,
                             Ref<const UIStyleDefinition> &accepted, std::string &error);
// Presentation-only writes, used on transient copies or explicit extraction drafts.
void ApplyUIStyle(UIAppearanceComponent &c, const UIStyleValues &v);
void ApplyUIStyle(UIRectComponent &c, const UIStyleValues &v);
void ApplyUIStyle(UITextStyleComponent &c, const UIStyleValues &v);
void ApplyUIStyle(TextComponent &c, const UIStyleValues &v);
void ApplyUIStyle(SpriteRendererComponent &c, const UIStyleValues &v);
void ApplyUIStyle(UIButtonComponent &c, const UIStyleValues &v);
void ApplyUIStyle(UITextEntryComponent &c, const UIStyleValues &v);
void ApplyUIStyle(UINineSliceComponent &c, const UIStyleValues &v);
template <class T> void ApplyUIStyle(T &, const UIStyleValues &)
{
}
void ExtractUIStyle(const UIAppearanceComponent &c, UIStyleValues &v);
void ExtractUIStyle(const UIRectComponent &c, UIStyleValues &v);
void ExtractUIStyle(const UITextStyleComponent &c, UIStyleValues &v);
void ExtractUIStyle(const TextComponent &c, UIStyleValues &v);
void ExtractUIStyle(const SpriteRendererComponent &c, UIStyleValues &v);
void ExtractUIStyle(const UIButtonComponent &c, UIStyleValues &v);
void ExtractUIStyle(const UITextEntryComponent &c, UIStyleValues &v);
void ExtractUIStyle(const UINineSliceComponent &c, UIStyleValues &v);
template <class T> void ExtractUIStyle(const T &, UIStyleValues &)
{
}
// Existing Inspector fields display effective presentation. Only changed typed
// groups become overrides; unchanged inherited fields never leak into authored data.
template <class T> uint32_t MergeUIStyleEdit(Entity e, const T &shown, const T &edited, T &authored)
{
    auto before = LocalUIStyle(e), after = before, local = before;
    ExtractUIStyle(shown, before);
    ExtractUIStyle(edited, after);
    uint32_t changed = 0;
#define ITEM(n)                                                                                    \
    if (!(before.n == after.n))                                                                    \
    {                                                                                              \
        local.n = after.n;                                                                         \
        changed |= UIStyleBit(UIStyleGroup::n);                                                    \
    }
    HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
    authored = edited;
    ApplyUIStyle(authored, local);
    return changed;
}
} // namespace Hazel
