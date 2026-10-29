#include "hzpch.h"
#include "RuntimeUI.h"
#include "UIStyleApplication.h"
namespace Hazel
{
const RuntimeUI::StyledPresentation *RuntimeUI::Styled(Entity e) const
{
    const auto &map = m_StyleBuilding ? m_StyleCandidate : m_Styles;
    auto it = map.find(e.GetUUID());
    return it == map.end() ? nullptr : &it->second;
}
void RuntimeUI::CaptureStyle(Entity e)
{
    if (!e.HasComponent<UIAppearanceComponent>())
    {
        m_StyleCandidate.erase(e.GetUUID());
        return;
    }
    const auto &config = e.GetComponent<UIAppearanceComponent>();
    config.Selection.Validate();
    if (config.Overrides & ~UIStyleAll)
        throw std::invalid_argument("Invalid style override presence");
    UIStyleValues cues;
    cues.Focus = config.Focus;
    cues.Selected = config.Selected;
    cues.Editing = config.Editing;
    cues.Validate();
    if (!config.Styled)
    {
        m_StyleCandidate.erase(e.GetUUID());
        return;
    }
    auto &next = m_StyleCandidate[e.GetUUID()];
    auto current = m_Styles.find(e.GetUUID());
    if (current != m_Styles.end())
        next = current->second;
    std::string error;
    UIStyleValues values;
    auto selection = e.GetComponent<UIAppearanceComponent>().Selection;
    try
    {
        values = ResolveUIStyle(e, m_Scene.GetAssets(), next.Asset, error);
    }
    catch (const std::exception &failure)
    {
        error = failure.what();
        next.Asset.reset();
        if (current != m_Styles.end() && current->second.Selection == selection &&
            current->second.Asset)
        {
            const auto& variants=current->second.Asset->Variants;
            auto previous=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.Name==selection.Variant;});
            if(previous!=variants.end()) {next.Asset=current->second.Asset;values=previous->Values;}
        }
        CopyUIStyleGroups(values, LocalUIStyle(e),
                          e.GetComponent<UIAppearanceComponent>().Overrides);
        values.Validate();
    }
    auto epoch = m_Scene.GetAssets() ? m_Scene.GetAssets()->Epoch() : 0;
    if (!(values.Image == next.Values.Image) || next.Epoch != epoch)
        next.Sprite.reset();
    next.Values = std::move(values);
    next.Epoch = epoch;
    next.Selection = selection;
    if (!error.empty())
        ControlProblem("Style #" + std::to_string(uint64_t(e.GetUUID())) + ": " + error +
                       " (last accepted variant or defaults retained)");
    next.Text.Font = next.Values.Font;
    next.Text.Color = next.Values.Foreground;
    next.Text.Kerning = next.Values.Spacing.x;
    next.Text.LineSpacing = next.Values.Spacing.y;
}
UIStyleValues RuntimeUI::Presentation(uint64_t id) const
{
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e || !m_Scene.IsEntityValid(id))
        throw std::invalid_argument("Style needs a live entity");
    if (auto p = Styled(e))
        return p->Values;
    return LocalUIStyle(e);
}
void RuntimeUI::ValidateStyles(bool dependencies) const
{
    for (auto handle : m_Scene.GetAllEntitiesWith<UIAppearanceComponent>())
    {
        Entity e(handle, &m_Scene);
        auto &c = e.GetComponent<UIAppearanceComponent>();
        c.Selection.Validate();
        if (c.Overrides & ~UIStyleAll)
            throw std::invalid_argument("Invalid UI local override presence");
        if (!e.HasComponent<UIRectComponent>())
            throw std::invalid_argument(
                "UI Appearance needs a Rect; assign styles to visual parts rather than canvases");
        if (!c.Styled)
            continue;
        Ref<const UIStyleDefinition> accepted;
        std::string error;
        try { (void)ResolveUIStyle(e, m_Scene.GetAssets(), accepted, error); }
        catch(const std::exception&) {
            // A retired variant is a repairable asset reference, not malformed
            // component data. Strict Play/export validation still rejects it.
            const bool missingVariant=accepted && std::none_of(accepted->Variants.begin(),accepted->Variants.end(),[&](const auto& v){return v.Name==c.Selection.Variant;});
            if(dependencies || !missingVariant)throw;
            continue;
        }
        if (dependencies && !error.empty())
            throw std::runtime_error("UI style #" + std::to_string(uint64_t(e.GetUUID())) + ": " +
                                     error);
        if (dependencies && accepted && !c.Selection.Asset.empty())
            AuditUIStyle(m_Scene.GetAssets()->Root(), *accepted);
    }
}
UIRectComponent RuntimeUI::RectFor(Entity e) const
{
    auto c = e.GetComponent<UIRectComponent>();
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
UITextStyleComponent RuntimeUI::TextStyleFor(Entity e) const
{
    auto c = e.HasComponent<UITextStyleComponent>() ? e.GetComponent<UITextStyleComponent>()
                                                    : UITextStyleComponent{};
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
UITextEntryComponent RuntimeUI::EntryFor(Entity e) const
{
    auto c = e.GetComponent<UITextEntryComponent>();
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
TextComponent &RuntimeUI::TextFor(Entity e)
{
    auto &map = m_StyleBuilding ? m_StyleCandidate : m_Styles;
    auto it = map.find(e.GetUUID());
    return it == map.end() ? e.GetComponent<TextComponent>() : it->second.Text;
}
SpriteRendererComponent RuntimeUI::SpriteFor(Entity e) const
{
    auto c = e.GetComponent<SpriteRendererComponent>();
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
UIButtonComponent RuntimeUI::ButtonFor(Entity e) const
{
    auto c = e.GetComponent<UIButtonComponent>();
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
UINineSliceComponent RuntimeUI::SliceFor(Entity e) const
{
    auto c = e.GetComponent<UINineSliceComponent>();
    if (auto p = Styled(e))
        ApplyUIStyle(c, p->Values);
    return c;
}
} // namespace Hazel
