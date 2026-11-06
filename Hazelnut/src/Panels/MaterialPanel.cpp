#include "MaterialPanel.h"
#include "UI/PropertyUI.h"
#include "Hazel/Project/Project.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
namespace Hazel
{
void MaterialPanel::Error(const std::exception &e)
{
    m_Error = e.what();
    if (ReportError)
        ReportError(m_Error);
}
void MaterialPanel::Close()
{
    m_Document.reset();
    m_Pickers = {};
    m_Previews = {};
    m_Open = m_Focused = m_Create = false;
    ++m_Identity;
    m_Error.clear();
}
bool MaterialPanel::Create(const std::filesystem::path &ref, const MaterialDefinition &value)
{
    if (!CanEdit())
        return false;
    try
    {
        auto candidate = CreateScope<MaterialDocument>(m_Assets);
        candidate->Create(ref, value);
        m_Document = std::move(candidate);
        m_Open = true;
        m_Create = false;
        m_Pickers = {};
        m_Previews = {};
        ++m_Identity;
        m_Error.clear();
        if (AssetsChanged)
            AssetsChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        Error(e);
        return false;
    }
}
bool MaterialPanel::Open(const std::filesystem::path &file)
{
    try
    {
        auto candidate = CreateScope<MaterialDocument>(m_Assets);
        candidate->Open(file.is_absolute() ? Project::MakeAssetReference(m_Assets->Root(), file)
                                           : file);
        m_Document = std::move(candidate);
        m_Open = true;
        m_Pickers = {};
        m_Previews = {};
        ++m_Identity;
        m_Error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        Error(e);
        return false;
    }
}
void MaterialPanel::BeginCreate(const std::filesystem::path &folder, bool duplicate)
{
    if (!CanEdit())
        return;
    m_CreateValues = duplicate && m_Document ? m_Document->Draft() : MaterialDefinition{};
    if (duplicate)
        m_CreateValues.Name += " copy";
    m_Destination =
        (folder / (duplicate ? "MaterialCopy.hmat" : "NewMaterial.hmat")).generic_u8string();
    m_Create = true;
    m_Error.clear();
}
bool MaterialPanel::Save()
{
    if (!CanEdit() || !m_Document)
        return false;
    try
    {
        m_Document->Save();
        m_Error.clear();
        if (AssetsChanged)
            AssetsChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        Error(e);
        return false;
    }
}
bool MaterialPanel::UndoRedo(bool redo)
{
    if (!CanEdit() || !m_Document)
        return false;
    try
    {
        if (redo)
            m_Document->Redo();
        else
            m_Document->Undo();
        m_Error.clear();
        m_Previews = {};
        return true;
    }
    catch (const std::exception &e)
    {
        Error(e);
        return false;
    }
}
void MaterialPanel::Properties()
{
    auto value = m_Document->Draft();
    PropertyUI::EditActivity activity;
    PropertyUI::ActivityScope scope(activity);
    PropertyUI::Text("name", "Name", value.Name);
    int model = int(value.Model);
    if (PropertyUI::Combo("model", "Model", model, "Metallic / roughness\0Unlit\0"))
        value.Model = MaterialModel(model);
    PropertyUI::Color("base", "Base color", glm::value_ptr(value.BaseColor));
    if (value.Model == MaterialModel::MetallicRoughness)
    {
        PropertyUI::SliderFloat("metallic", "Metallic", value.Metallic, 0, 1);
        PropertyUI::SliderFloat("roughness", "Roughness", value.Roughness, 0, 1);
        auto emissive = glm::vec4(value.Emissive, 1);
        if (PropertyUI::Color("emissive", "Emissive", glm::value_ptr(emissive), {}, false))
            value.Emissive = glm::vec3(emissive);
        PropertyUI::DragFloat("normal", "Normal scale", value.NormalScale, .01f, -16, 16);
        PropertyUI::SliderFloat("occlusion", "Occlusion strength", value.OcclusionStrength, 0, 1);
    }
    int alpha = int(value.Alpha);
    if (PropertyUI::Combo("alpha", "Alpha", alpha, "Opaque\0Mask\0"))
        value.Alpha = MaterialAlpha(alpha);
    if (value.Alpha == MaterialAlpha::Mask)
        PropertyUI::SliderFloat("cutoff", "Alpha cutoff", value.AlphaCutoff, 0, 1);
    PropertyUI::Checkbox("sided", "Double sided", value.DoubleSided);
    ImGui::Separator();
    ImGui::TextUnformatted("Textures — UV0");
    for (unsigned i = 0; i < 5; ++i)
    {
        if (value.Model == MaterialModel::Unlit && i)
            continue;
        ImGui::PushID(int(i));
        auto &texture = value.Textures[i];
        auto before = texture;
        AssetPicker("image", MaterialTextureName(MaterialTexture(i)), texture.Image, "Image",
                    m_Assets, m_Pickers[i]);
        auto encoding = MaterialTextureEncoding(MaterialTexture(i));
        ImGui::TextDisabled("%s", encoding == TextureEncoding::SRGB
                                      ? "sRGB color; alpha stays linear"
                                      : "Linear data; channels remain unchanged");
        if (ImGui::TreeNode("Sampling and source preview"))
        {
            int min = int(texture.Min), mag = int(texture.Mag), s = int(texture.S),
                t = int(texture.T);
            if (PropertyUI::Combo("min", "Minification", min,
                                  "Nearest\0Linear\0Nearest mip / nearest\0Linear mip / "
                                  "nearest\0Nearest mip / linear\0Linear mip / linear\0"))
                texture.Min = TextureFilter(min);
            if (PropertyUI::Combo("mag", "Magnification", mag, "Nearest\0Linear\0"))
                texture.Mag = TextureFilter(mag);
            if (PropertyUI::Combo("s", "Wrap S", s, "Repeat\0Clamp to edge\0Mirrored repeat\0"))
                texture.S = TextureWrap(s);
            if (PropertyUI::Combo("t", "Wrap T", t, "Repeat\0Clamp to edge\0Mirrored repeat\0"))
                texture.T = TextureWrap(t);
            if (!texture.Image.empty())
                try
                {
                    if (!(before == texture))
                        m_Previews[i].reset();
                    if (!m_Previews[i])
                        m_Previews[i] = Texture2D::Create(
                            Project::ResolveOwnedAsset(m_Assets->Root(), texture.Image)
                                .generic_u8string(),
                            TextureSpecification::ImGuiFile());
                    auto &image = m_Previews[i];
                    float width = std::min(180.f, ImGui::GetContentRegionAvail().x);
                    ImGui::Image((ImTextureID)(uintptr_t)image->GetRendererID(),
                                 {width, width * image->GetHeight() / image->GetWidth()}, {0, 0},
                                 {1, 1});
                }
                catch (const std::exception &e)
                {
                    PropertyUI::Validation(e.what());
                }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    try
    {
        if (activity.Changed)
        {
            m_Document->Preview(std::move(value));
            m_Error.clear();
            m_Previews = {};
        }
        if (m_Document->Active() &&
            (activity.Committed || (!ImGui::IsAnyItemActive() &&
                                    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))))
            m_Document->Finish();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_Document->Finish(true);
    }
    catch (const std::exception &e)
    {
        Error(e);
    }
}
void MaterialPanel::Render(bool editable)
{
    if (m_Create)
    {
        ImGui::OpenPopup("Create material");
        m_Create = false;
    }
    if (ImGui::BeginPopupModal("Create material", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(!editable);
        PropertyUI::Text("destination", "Project .hmat", m_Destination);
        PropertyUI::Text("name", "Name", m_CreateValues.Name);
        ImGui::TextWrapped("Creates an editable native material. Metallic/roughness uses direct "
                           "lighting and scene-referred presentation.");
        if (ImGui::Button("Create"))
        {
            auto path = std::filesystem::u8path(m_Destination);
            auto values = m_CreateValues;
            auto action = [this, path, values] { return Create(path, values); };
            if (ReplaceDocument)
                ReplaceDocument(action);
            else
                action();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        PropertyUI::Validation(m_Error.c_str());
        ImGui::EndPopup();
    }
    m_Focused = false;
    if (!m_Open || !m_Document)
        return;
    bool visible = true;
    auto title = std::string("Material: ") + Name() + (Dirty() ? " *" : "") + "###Material";
    ImGui::SetNextWindowSize({520, 650}, ImGuiCond_FirstUseEver);
    bool contents = ImGui::Begin(title.c_str(), &visible);
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!visible && RequestClose)
        RequestClose();
    if (contents)
    {
        ImGui::TextWrapped("Native material document. Inspect on a mesh in the scene viewport; "
                           "direct lighting has no ambient/IBL.");
        ImGui::BeginDisabled(!editable || (GuardPending && GuardPending()));
        if (ImGui::Button("Save"))
            Save();
        ImGui::SameLine();
        if (ImGui::Button("Duplicate..."))
            BeginCreate(m_Document->Reference().parent_path(), true);
        ImGui::SameLine();
        if (ImGui::Button("Reopen / discard") && ReplaceDocument)
        {
            auto path = m_Document->File().Path();
            ReplaceDocument([this, path] { return Open(path); });
        }
        ImGui::BeginDisabled(!CanUndo());
        if (ImGui::Button("Undo"))
            UndoRedo(false);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!CanRedo());
        if (ImGui::Button("Redo"))
            UndoRedo(true);
        ImGui::EndDisabled();
        Properties();
        ImGui::EndDisabled();
        PropertyUI::Validation(m_Error.c_str());
    }
    ImGui::End();
}
} // namespace Hazel
