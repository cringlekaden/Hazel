#include "PropertyUI.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <misc/cpp/imgui_stdlib.h>

namespace Hazel::PropertyUI
{
void Help(const char *text)
{
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
void Validation(const char *text)
{
    if (!text || !*text)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, .64f, .36f, 1.f});
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
void WrapButton(const char *label)
{
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(label).x +
            ImGui::GetStyle().FramePadding.x * 2 <=
        right)
        ImGui::SameLine();
}
Row::Row(const char *key, const char *label, Options options) : m_Options(options)
{
    // Hover help adds meaning; visible labels already identify the property.
    if (m_Options.Help && label && std::strcmp(m_Options.Help, label) == 0)
        m_Options.Help = nullptr;
    ImGui::PushID(key);
    const float width = ImGui::GetContentRegionAvail().x, font = ImGui::GetFontSize();
    m_Table = ImGui::BeginTable("##property", 2,
                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    if (m_Table)
    {
        const float labelWidth = std::min(std::clamp(width * .35f, font * 8.f, font * 14.f),
                                          std::max(font * 3.f, width * .45f));
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
        // Explicit weight avoids content-derived 0/0 sizing on a newly appearing row.
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped("%s", label);
        Help(m_Options.Help);
        ImGui::TableSetColumnIndex(1);
    }
    ImGui::BeginDisabled(options.DisabledReason && *options.DisabledReason);
    Width();
}
Row::~Row()
{
    ImGui::EndDisabled();
    Validation(m_Options.Validation);
    if (m_Options.DisabledReason && *m_Options.DisabledReason)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", m_Options.DisabledReason);
        ImGui::PopStyleColor();
    }
    if (m_Table)
        ImGui::EndTable();
    ImGui::PopID();
}
EditResult Row::Result(bool changed, bool immediate) const
{
    const bool committed = ImGui::IsItemDeactivatedAfterEdit() || (changed && immediate);
    Help(m_Options.DisabledReason ? m_Options.DisabledReason : m_Options.Help);
    return {changed, committed, false};
}
void Row::Width(bool reset) const
{
    const float reserve = reset ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0;
    ImGui::SetNextItemWidth(std::max(1.f, ImGui::GetContentRegionAvail().x - reserve));
}
bool Row::Reset(bool available) const
{
    if (!available)
        return false;
    ImGui::SameLine();
    const bool clicked = ImGui::Button("R##reset", {ImGui::GetFrameHeight(), 0});
    Help("Reset to the owner-provided default");
    return clicked;
}
template <typename T>
static EditResult ResetResult(Row &row, EditResult result, T &value, const T *defaults)
{
    if (row.Reset(defaults != nullptr))
    {
        value = *defaults;
        result = {true, true, true};
    }
    return result;
}
EditResult DragFloat(const char *key, const char *label, float &value, float speed, float minimum,
                     float maximum, const char *format, const float *reset, Options options)
{
    Row row(key, label, options);
    row.Width(reset);
    auto result = row.Result(ImGui::DragFloat("##value", &value, speed, minimum, maximum, format));
    return ResetResult(row, result, value, reset);
}
EditResult DragInt(const char *key, const char *label, int &value, float speed, int minimum, int maximum,
                   const int *reset, Options options)
{
    Row row(key, label, options);
    row.Width(reset);
    auto result = row.Result(ImGui::DragInt("##value", &value, speed, minimum, maximum));
    return ResetResult(row, result, value, reset);
}
EditResult SliderFloat(const char *key, const char *label, float &value, float minimum, float maximum,
                       const char *format, const float *reset, Options options, ImGuiSliderFlags flags)
{
    Row row(key, label, options);
    row.Width(reset);
    auto result = row.Result(ImGui::SliderFloat("##value", &value, minimum, maximum, format, flags));
    return ResetResult(row, result, value, reset);
}
EditResult SliderInt(const char *key, const char *label, int &value, int minimum, int maximum,
                     const int *reset, Options options)
{
    Row row(key, label, options);
    row.Width(reset);
    auto result = row.Result(ImGui::SliderInt("##value", &value, minimum, maximum));
    return ResetResult(row, result, value, reset);
}
EditResult Checkbox(const char *key, const char *label, bool &value, const bool *reset, Options options)
{
    Row row(key, label, options);
    auto result = row.Result(ImGui::Checkbox("##value", &value), true);
    return ResetResult(row, result, value, reset);
}
EditResult Combo(const char *key, const char *label, int &value, const char *items, Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::Combo("##value", &value, items), true);
}
EditResult Text(const char *key, const char *label, std::string &draft, Options options,
                ImGuiInputTextFlags flags)
{
    Row row(key, label, options);
    return row.Result(ImGui::InputText("##value", &draft, flags));
}
EditResult Multiline(const char *key, const char *label, std::string &draft, Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::InputTextMultiline(
        "##value", &draft, {ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 4}));
}
EditResult Scalar(const char *key, const char *label, ImGuiDataType type, void *value, Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::InputScalar("##value", type, value));
}
EditResult Double(const char *key, const char *label, double &value, double step, const char *format,
                  Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::InputDouble("##value", &value, step, step * 10, format));
}
EditResult Vector(const char *key, const char *label, float *value, int axes, float speed,
                  const float *defaults, const char *format, Options options, float minimum,
                  float maximum)
{
    Row row(key, label, options);
    EditResult result;
    const float width = ImGui::GetContentRegionAvail().x, button = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float minimumNumber = std::max(ImGui::GetFontSize() * 3.5f,
                                        ImGui::CalcTextSize("-000.00").x + ImGui::GetStyle().FramePadding.x * 2);
    const int perLine = std::clamp(int((width + spacing) / (button + minimumNumber + spacing)), 1, axes);
    const float group = (width - spacing * (perLine - 1)) / perLine;
    const char *names[] = {"X", "Y", "Z", "W"};
    const ImVec4 colors[] = {{.8f, .1f, .15f, 1}, {.2f, .7f, .2f, 1},
                            {.1f, .25f, .8f, 1}, {.55f, .3f, .7f, 1}};
    for (int axis = 0; axis < axes; ++axis)
    {
        ImGui::PushID(axis);
        if (axis % perLine)
            ImGui::SameLine(0, spacing);
        const auto color = colors[axis];
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {color.x + .1f, color.y + .1f, color.z + .1f, 1});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {color.x * .8f, color.y * .8f, color.z * .8f, 1});
        auto *bold = ImGui::GetIO().Fonts->Fonts[0];
        ImGui::PushFont(bold);
        ImGui::BeginDisabled(!defaults);
        if (ImGui::Button(names[axis], {button, button}))
        {
            const auto reset = ResetAxis(value, axes, axis, defaults);
            result.Changed |= reset.Changed;
            result.Committed |= reset.Committed;
            result.ResetRequested |= reset.ResetRequested;
        }
        char hint[128];
        if (defaults) std::snprintf(hint, sizeof(hint), "Reset %s to the owner default: %.3f", names[axis], defaults[axis]);
        else std::snprintf(hint, sizeof(hint), "%s axis; no reset default supplied", names[axis]);
        Help(options.DisabledReason ? options.DisabledReason : hint);
        ImGui::EndDisabled();
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 0); // Recognizable axis button attached immediately to its numeric field.
        ImGui::SetNextItemWidth(std::max(1.f, group - button));
        auto edit =
            row.Result(ImGui::DragFloat("##value", value + axis, speed, minimum, maximum, format));
        result.Changed |= edit.Changed;
        result.Committed |= edit.Committed;
        ImGui::PopID();
    }
    return result;
}
EditResult ResetAxis(float *value, int axes, int axis, const float *defaults)
{
    if (!value || !defaults || axes < 1 || axes > 4 || axis < 0 || axis >= axes) return {};
    const bool changed = value[axis] != defaults[axis];
    value[axis] = defaults[axis];
    return {changed, true, true};
}
EditResult SliderVector2(const char *key, const char *label, float *value, float minimum, float maximum,
                         Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::SliderFloat2("##value", value, minimum, maximum));
}
EditResult Color(const char *key, const char *label, float *value, Options options)
{
    Row row(key, label, options);
    return row.Result(ImGui::ColorEdit4("##value", value));
}
void ReadOnly(const char *key, const char *label, const char *value, Options options)
{
    Row row(key, label, options);
    ImGui::TextWrapped("%s", value);
    // Wrapped information already exposes the full value. Keep behavioral help.
    if (options.Help && (!value || std::strcmp(options.Help, value) != 0) &&
        (!label || std::strcmp(options.Help, label) != 0))
        Help(options.Help);
}
ReferenceAction Reference(const char *key, const char *label, const char *value, bool assigned,
                          Options options)
{
    Row row(key, label, options);
    ImGui::TextWrapped("%s", value);
    ReferenceAction action = ReferenceAction::None;
    if (ImGui::SmallButton("Choose..."))
        action = ReferenceAction::Browse;
    if (assigned)
    {
        if (ImGui::GetContentRegionAvail().x > ImGui::GetFontSize() * 12)
            ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
            action = ReferenceAction::Clear;
        if (ImGui::SmallButton("Open"))
            action = ReferenceAction::Open;
        ImGui::SameLine();
        if (ImGui::SmallButton("Reveal"))
            action = ReferenceAction::Reveal;
    }
    return action;
}
} // namespace Hazel::PropertyUI
