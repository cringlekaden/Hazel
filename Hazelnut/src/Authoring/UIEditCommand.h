#pragma once
#include "DocumentHistory.h"
#include "Hazel/Scene/Entity.h"
#include <optional>
namespace Hazel
{
struct UITextValue
{
    std::string Text;
    glm::vec4 Color{1};
    float Kerning = 0, LineSpacing = 0;
    FontSelection Font;
};
struct UIImageValue
{
    glm::vec4 Color{1};
    SpriteSource Source;
};
// Only authored values. Native SpriteSource resources are stripped on capture
// and command construction; no font, GPU, registry or managed object is retained.
struct UIEditValues
{
    std::optional<UICanvasComponent> Canvas;
    std::optional<UIRectComponent> Rect;
    std::optional<UILayoutComponent> Flow;
    std::optional<UITextStyleComponent> Style;
    std::optional<UINineSliceComponent> Slice;
    std::optional<UIButtonComponent> Button;
    std::optional<UIRadioGroupComponent> Group;
    std::optional<UIProgressComponent> Progress;
    std::optional<UITextValue> Text;
    std::optional<UIImageValue> Image;
    static UIEditValues Capture(Entity entity);
    void StripResources();
    size_t Bytes() const;
    bool Same(const UIEditValues &other) const;
    template <class T> void Set(const T &value)
    {
        if constexpr (std::is_same_v<T, UICanvasComponent>)
            Canvas = value;
        else if constexpr (std::is_same_v<T, UIRectComponent>)
            Rect = value;
        else if constexpr (std::is_same_v<T, UILayoutComponent>)
            Flow = value;
        else if constexpr (std::is_same_v<T, UITextStyleComponent>)
            Style = value;
        else if constexpr (std::is_same_v<T, UINineSliceComponent>)
            Slice = value;
        else if constexpr (std::is_same_v<T, UIButtonComponent>)
            Button = value;
        else if constexpr (std::is_same_v<T, UIRadioGroupComponent>) Group=value;
        else if constexpr (std::is_same_v<T, UIProgressComponent>) Progress=value;
        else if constexpr (std::is_same_v<T, TextComponent>)
            Text = UITextValue{value.TextString, value.Color, value.Kerning, value.LineSpacing, value.Font};
        else if constexpr (std::is_same_v<T, SpriteRendererComponent>)
            Image = UIImageValue{value.Color, value.Source};
        StripResources();
    }
};
class UIEditCommand final : public DocumentCommand
{
  public:
    UIEditCommand(Scene &scene, uint64_t entity, std::string name, UIEditValues before);
    bool Preview(Scene &scene, UIEditValues proposed, std::string &error);
    bool Preview(Scene &scene, const UIEditValues &sampleBefore, UIEditValues proposed,
                 std::string &error);
    const std::string &Name() const override
    {
        return m_Name;
    }
    size_t Bytes() const override;
    bool Changed() const override
    {
        return !m_Before.Same(m_After);
    }
    void Check(Scene &scene) const override;
    void Apply(Scene &scene, bool undo) override;
    void Cancel(Scene &scene) override;
    uint64_t EntityID() const
    {
        return m_Entity;
    }

  private:
    Entity Target(Scene &scene, bool cancelling = false) const;
    void Publish(Scene &scene, const UIEditValues &current, UIEditValues next,
                 bool cancelling = false) const;
    uint64_t m_Document, m_Entity;
    std::vector<std::pair<uint64_t, uint64_t>> m_Guards;
    std::array<uint64_t, 10> m_ComponentVersions{};
    std::string m_Name;
    UIEditValues m_Before, m_After;
};
} // namespace Hazel
