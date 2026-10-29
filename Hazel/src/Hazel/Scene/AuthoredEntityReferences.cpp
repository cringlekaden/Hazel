#include "hzpch.h"
#include "AuthoredEntityReferences.h"
#include "Entity.h"

namespace Hazel {
void VisitAuthoredEntityReferences(Entity entity, ScriptFieldMap& fields,
    const std::function<void(uint64_t&, std::string_view)>& visit) {
    if (entity.HasComponent<UICanvasComponent>()) {
        auto& c = entity.GetComponent<UICanvasComponent>().Screen;
        visit(c.InitialFocus, "Screen.InitialFocus");
        visit(c.DefaultAction, "Screen.DefaultAction");
        visit(c.CancelTarget, "Screen.CancelTarget");
        visit(c.ReturnFocus, "Screen.ReturnFocus");
        visit(c.EventOwner, "Screen.EventOwner");
    }
    if (entity.HasComponent<UIButtonComponent>()) {
        auto& c = entity.GetComponent<UIButtonComponent>();
        visit(c.Indicator, "Button.Indicator");
        visit(c.Group, "Button.Group");
        visit(c.EventOwner, "Button.EventOwner");
    }
    if (entity.HasComponent<UIRadioGroupComponent>()) {
        auto& c = entity.GetComponent<UIRadioGroupComponent>();
        visit(c.InitialSelection, "RadioGroup.InitialSelection");
        visit(c.EventOwner, "RadioGroup.EventOwner");
    }
    if (entity.HasComponent<UITabComponent>()) {
        auto& c = entity.GetComponent<UITabComponent>();
        visit(c.Page, "Tab.Page");
        visit(c.InitialFocus, "Tab.InitialFocus");
    }
    if (entity.HasComponent<UITooltipComponent>())
        visit(entity.GetComponent<UITooltipComponent>().Body, "Tooltip.Body");
    if (entity.HasComponent<UIDropdownComponent>()) {
        auto& c = entity.GetComponent<UIDropdownComponent>();
        visit(c.Trigger, "Dropdown.Trigger");
        visit(c.Popup, "Dropdown.Popup");
    }
    if (entity.HasComponent<UIScrollViewComponent>()) {
        auto& c = entity.GetComponent<UIScrollViewComponent>();
        visit(c.Content, "ScrollView.Content");
        visit(c.HorizontalTrack, "ScrollView.HorizontalTrack");
        visit(c.HorizontalThumb, "ScrollView.HorizontalThumb");
        visit(c.VerticalTrack, "ScrollView.VerticalTrack");
        visit(c.VerticalThumb, "ScrollView.VerticalThumb");
    }
    if (entity.HasComponent<UISliderComponent>()) {
        auto& c = entity.GetComponent<UISliderComponent>();
        visit(c.Track, "Slider.Track");
        visit(c.Fill, "Slider.Fill");
        visit(c.Thumb, "Slider.Thumb");
    }
    if (entity.HasComponent<UIProgressComponent>())
        visit(entity.GetComponent<UIProgressComponent>().Fill, "Progress.Fill");
    for (auto& [name, field] : fields) {
        if (field.Field.Type != ScriptFieldType::Entity)
            continue;
        auto value = field.GetValue<uint64_t>();
        const auto before = value;
        visit(value, name);
        if (value != before)
            field.SetValue(value);
    }
}
}
