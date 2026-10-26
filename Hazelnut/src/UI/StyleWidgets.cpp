#include "StyleWidgets.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Project/Project.h"
#include "PropertyUI.h"
#include <glm/gtc/type_ptr.hpp>
namespace Hazel
{
bool StyleValuesEditor(UIStyleValues &v, StyleWidgetState &state)
{
    bool changed = false;
    if (ImGui::TreeNode("Typography"))
    {
        std::string family = v.Font.Family.generic_u8string();
        if (PropertyUI::Text("family", "Family asset", family))
        {
            v.Font.Family = std::filesystem::u8path(family);
            changed = true;
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (auto payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                auto path = ContentBrowserPath(payload->Data, payload->DataSize);
                if (path.extension() == ".hfont")
                {
                    v.Font.Family = Project::MakeAssetReference(Project::GetAssetDirectory(), path);
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        int face = int(v.Font.Style);
        if (PropertyUI::Combo("face", "Face style", face, "Regular\0Bold\0Italic\0Bold Italic\0"))
        {
            v.Font.Style = FontStyle(face);
            changed = true;
        }
        changed |= PropertyUI::DragFloat("size", "Text size", v.Size, .25f, .01f, 512);
        changed |= PropertyUI::Vector("spacing", "Tracking / line spacing",
                                      glm::value_ptr(v.Spacing), 2, .01f);
        PropertyUI::Help("Empty family uses shipped Open Sans. Size uses logical font-height "
                         "pixels; spacing uses font-height units (tracking -8..8, additional line "
                         "spacing 0..8). Wrap/alignment/overflow belong to each text element.");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Colors and cues"))
    {
        for (auto item : {std::pair<const char *, glm::vec4 *>{"Foreground", &v.Foreground},
                          {"Background", &v.Background},
                          {"Focus outline", &v.Focus},
                          {"Selected indicator tint", &v.Selected},
                          {"Editing outline", &v.Editing},
                          {"Text caret", &v.Caret},
                          {"Text selection", &v.Selection}})
            changed |= PropertyUI::Color(item.first, item.first, glm::value_ptr(*item.second));
        PropertyUI::Help(
            "Focus/selected/editing remain separate geometry/visibility cues. Selected tints the "
            "named indicator; it does not change selection or replace its shape.");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Images and five button states"))
    {
        auto editImage = [&](SpriteSource &source, size_t slot) {
            SpriteRendererComponent c;
            c.Source = source;
            SpriteSourceEditor(c, state.Images[slot]);
            if (!(source == c.Source))
            {
                source = c.Source;
                changed = true;
            }
        };
        ImGui::TextUnformatted("Base background");
        editImage(v.Image, 0);
        const char *states[] = {"Normal", "Hover", "Pressed", "Disabled", "Focused"};
        for (size_t i = 0; i < std::size(states); ++i)
        {
            ImGui::PushID(int(i));
            if (ImGui::TreeNode(states[i]))
            {
                changed |= PropertyUI::Vector("tint", "Tint multiplier",
                                              glm::value_ptr(v.States.Colors[i]), 4, .01f, nullptr,
                                              "%.2f", {}, 0, 4);
                editImage(v.States.Images[i], i + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Metrics"))
    {
        changed |= PropertyUI::Vector("padding", "Padding L/T/R/B", glm::value_ptr(v.Padding), 4,
                                      .25f, nullptr, "%.2f", {}, 0, 1000000);
        changed |=
            PropertyUI::Vector("borders", "Slice source borders", glm::value_ptr(v.Slice.Borders),
                               4, .25f, nullptr, "%.2f", {}, 0, 1000000);
        changed |= PropertyUI::DragFloat("slice-scale", "Slice border scale", v.Slice.Scale, .1f,
                                         .01f, 10000);
        PropertyUI::Help("Padding belongs to the element's existing Rect. Slice fields apply only "
                         "when it already has Nine Slice; styles never add components, margins, "
                         "sizing modes or part relationships.");
        ImGui::TreePop();
    }
    return changed;
}
} // namespace Hazel
