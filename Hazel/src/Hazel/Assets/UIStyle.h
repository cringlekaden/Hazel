#pragma once
#include "FontFamily.h"
#include "SpriteSheet.h"
#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
// A fixed typed vocabulary. No selectors, property paths or inheritance.
#define HZ_UI_STYLE_FIELDS(F)                                                                      \
    F(Font)                                                                                        \
    F(Size) F(Spacing) F(Foreground) F(Background) F(Image) F(States) F(Padding) F(Slice) F(Focus) \
        F(Selected) F(Editing) F(Caret) F(Selection)
enum class UIStyleGroup : uint8_t
{
#define ITEM(n) n,
    HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
    Count
};
constexpr uint32_t UIStyleBit(UIStyleGroup group)
{
    return 1u << unsigned(group);
}
constexpr uint32_t UIStyleAll = (1u << unsigned(UIStyleGroup::Count)) - 1;
const char *UIStyleGroupName(UIStyleGroup group);
struct UIStyleStates
{
    std::array<glm::vec4, 5> Colors{{glm::vec4(1),
                                     {1.15f, 1.15f, 1.15f, 1},
                                     {.75f, .75f, .75f, 1},
                                     glm::vec4(.5f),
                                     {1.2f, 1.15f, .8f, 1}}};
    std::array<SpriteSource, 5> Images;
    bool operator==(const UIStyleStates &b) const
    {
        return Colors == b.Colors && Images == b.Images;
    }
};
struct UIStyleSlice
{
    glm::vec4 Borders{4};
    float Scale = 1;
    bool operator==(const UIStyleSlice &b) const
    {
        return Borders == b.Borders && Scale == b.Scale;
    }
};
struct UIStyleValues
{
    FontSelection Font;
    float Size = 24;
    glm::vec2 Spacing{}; // tracking / additional line spacing, font-height units
    glm::vec4 Foreground{1}, Background{1};
    SpriteSource Image;
    UIStyleStates States;
    glm::vec4 Padding{};
    UIStyleSlice Slice;
    glm::vec4 Focus{1, .9f, .65f, 1}, Selected{1}, Editing{.3f, 1, 1, 1}, Caret{1},
        Selection{.18f, .42f, .8f, .5f};
    void Validate() const;
    bool operator==(const UIStyleValues &b) const;
};
template <class T, class F> void VisitUIStyle(T &v, F f)
{
#define ITEM(n) f(UIStyleGroup::n, v.n);
    HZ_UI_STYLE_FIELDS(ITEM)
#undef ITEM
}
void CopyUIStyleGroups(UIStyleValues &target, const UIStyleValues &source, uint32_t groups);
struct UIStyleSelection
{
    std::filesystem::path Asset; // empty selects the shipped engine style
    std::string Variant = "Control";
    void Validate() const;
    bool operator==(const UIStyleSelection &b) const
    {
        return Asset == b.Asset && Variant == b.Variant;
    }
    bool operator!=(const UIStyleSelection &b) const
    {
        return !(*this == b);
    }
};
struct UIStyleVariant
{
    std::string Name = "Control";
    UIStyleValues Values;
};
struct UIStyleDefinition
{
    std::string Name = "New UI style";
    std::vector<UIStyleVariant> Variants{{}};
    void Validate() const;
    const UIStyleValues &Find(const std::string &name) const;
};
UIStyleDefinition ReadUIStyleText(const std::string &text);
UIStyleDefinition ReadUIStyle(const std::filesystem::path &file);
std::string WriteUIStyleText(const UIStyleDefinition &definition);
const UIStyleDefinition &DefaultUIStyle();
// Shared typed codec also used for extraction and precise test fixtures.
UIStyleValues ReadUIStyleValues(const YAML::Node &node);
void WriteUIStyleValues(YAML::Emitter &out, const UIStyleValues &values);
std::vector<std::filesystem::path> AuditUIStyle(const std::filesystem::path &root,
                                                const UIStyleDefinition &definition);
} // namespace Hazel
