#include "EnvironmentPanel.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "UI/PropertyUI.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
namespace Hazel
{
void EnvironmentPanel::Error(const std::exception &e)
{
    m_Error = e.what();
    if (ReportError)
        ReportError(m_Error);
}
void EnvironmentPanel::Close()
{
    m_Document.reset();
    m_Picker = {};
    m_StatusEpoch = 0;
    m_Open = m_Focused = m_Create = false;
    ++m_Identity;
    m_Error.clear();
    m_Status.clear();
    m_SourceImport.clear();
    m_SourceDestination.clear();
}
bool EnvironmentPanel::Create(const std::filesystem::path &ref, const EnvironmentDefinition &value)
{
    if (!CanEdit())
        return false;
    try
    {
        auto candidate = CreateScope<EnvironmentDocument>(m_Assets);
        candidate->Create(ref, value);
        m_Document = std::move(candidate);
        m_Open = true;
        m_Create = false;
        m_Picker = {};
        m_StatusEpoch = 0;
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
bool EnvironmentPanel::Open(const std::filesystem::path &file)
{
    try
    {
        auto candidate = CreateScope<EnvironmentDocument>(m_Assets);
        candidate->Open(file.is_absolute() ? Project::MakeAssetReference(m_Assets->Root(), file)
                                           : file);
        m_Document = std::move(candidate);
        m_Open = true;
        m_Picker = {};
        m_StatusEpoch = 0;
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
void EnvironmentPanel::BeginCreate(const std::filesystem::path &folder, bool duplicate,
                                   const std::filesystem::path &image)
{
    if (!CanEdit())
        return;
    m_CreateValues = duplicate && m_Document ? m_Document->Draft() : EnvironmentDefinition{};
    if (duplicate)
        m_CreateValues.Name += " copy";
    if (!image.empty())
        m_CreateValues.Image = image;
    m_Picker = {};
    m_SourceImport.clear();
    m_SourceDestination.clear();
    m_Destination =
        (folder / (duplicate ? "EnvironmentCopy.henv" : "NewEnvironment.henv")).generic_u8string();
    m_Create = true;
    m_Error.clear();
}
bool EnvironmentPanel::Save()
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
bool EnvironmentPanel::UndoRedo(bool redo)
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
        m_StatusEpoch = 0;
        return true;
    }
    catch (const std::exception &e)
    {
        Error(e);
        return false;
    }
}
void EnvironmentPanel::Inspect()
{
    m_StatusEpoch = m_Assets->Epoch();
    m_Status.clear();
    if (!m_Document)
        return;
    auto resolution = m_Assets->Environment(m_Document->Reference());
    if (resolution.Accepted)
    {
        auto &e = *resolution.Accepted;
        m_Status = "Accepted " + AssetKey(e.Generation) + " / " + e.Identity + "\nBackground " +
                   std::to_string(e.Background.Side) + "; irradiance " +
                   std::to_string(e.Irradiance.Side) + "; specular " +
                   std::to_string(e.Specular.Side) + " / " +
                   std::to_string(e.Specular.Mips.size()) + " explicit mips; LUT " +
                   std::to_string(e.BRDF.Side) + "\nLUT identity " + e.LUTIdentity + "\n" +
                   std::to_string(e.Seconds) + " s; peak resident " + std::to_string(e.PeakBytes) +
                   " bytes\nRGB32F cubes / RG32F LUT; " + e.Producer;
    }
    if (!resolution.Error.empty())
        m_Status += "\nPreparation required: " + resolution.Error;
    if (resolution.Accepted)
        try
        {
            const auto &d = m_Document->Draft();
            auto source = ReadAssetBytes(Project::ResolveOwnedAsset(m_Assets->Root(), d.Image),
                                         64 * 1024 * 1024);
            if (AssetHash(source) != resolution.Accepted->SourceHash)
                m_Status += "\nSTALE SOURCE: HDR bytes changed. Prepare to accept a replacement; "
                            "accepted generation remains available.";
        }
        catch (const std::exception &e)
        {
            m_Status += "\nProducer input unavailable: " + std::string(e.what()) +
                        ". Accepted resources run without it.";
        }
}
namespace
{
void Resolution(const char *key, const char *label, uint32_t &value, unsigned maximum)
{
    PropertyUI::Row row(key, label);
    if (ImGui::BeginCombo("##value", std::to_string(value).c_str()))
    {
        for (unsigned n = 4; n <= maximum; n *= 2)
            if (ImGui::Selectable(std::to_string(n).c_str(), n == value))
                value = n;
        ImGui::EndCombo();
    }
}
} // namespace
void EnvironmentPanel::Properties()
{
    auto value = m_Document->Draft();
    PropertyUI::EditActivity activity;
    PropertyUI::ActivityScope scope(activity);
    PropertyUI::Text("name", "Name", value.Name);
    ImGui::SeparatorText("Image source");
    AssetPicker("source", "HDR source", value.Image, ".hdr", m_Assets, m_Picker);
    ImGui::TextWrapped("Linear Rec.709 / D65 Radiance HDR; top-down equirectangular rows. Source "
                       "is needed only to prepare, not to run accepted lighting.");
    if (ImGui::TreeNode("Preparation quality"))
    {
        Resolution("background", "Background face", value.Profile.Background, 512);
        Resolution("specular", "Specular face", value.Profile.Specular, 256);
        Resolution("irradiance", "Irradiance face", value.Profile.Irradiance, 64);
        {
            PropertyUI::Row row("samples", "Convolution samples");
            if (ImGui::BeginCombo("##value", std::to_string(value.Profile.Samples).c_str()))
            {
                for (unsigned n = 64; n <= 4096; n *= 2)
                    if (ImGui::Selectable(std::to_string(n).c_str(), n == value.Profile.Samples))
                        value.Profile.Samples = n;
                ImGui::EndCombo();
            }
        }
        ImGui::TextWrapped(
            "Explicit GGX mip chain; shared 128-square BRDF LUT uses 4096 samples. Changes require "
            "Prepare. Yaw, gain and exposure belong to Scene Settings.");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Provenance"))
    {
        PropertyUI::Text("notice", "Source attribution", value.Notice);
        ImGui::TreePop();
    }
    try
    {
        // AssetPicker/combos also participate even if a custom row has no gesture.
        if (!(value == m_Document->Draft()))
            m_Document->Preview(std::move(value));
        if (m_Document->Active() && (activity.Committed || !ImGui::IsAnyItemActive()))
            m_Document->Finish();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_Document->Finish(true);
    }
    catch (const std::exception &e)
    {
        Error(e);
    }
}
void EnvironmentPanel::Render(bool editable)
{
    if (m_Create)
    {
        ImGui::OpenPopup("Create environment");
        m_Create = false;
    }
    if (ImGui::BeginPopupModal("Create environment", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(!editable);
        PropertyUI::Text("destination", "Project .henv", m_Destination);
        PropertyUI::Text("name", "Name", m_CreateValues.Name);
        AssetPicker("source", "HDR source", m_CreateValues.Image, ".hdr", m_Assets, m_Picker);
        if (ImGui::Button("Import HDR source..."))
        {
            auto file = FileDialogs::OpenFile("Radiance HDR (*.hdr)\0*.hdr\0");
            if (!file.empty())
            {
                m_SourceImport = file;
                m_SourceDestination = (std::filesystem::u8path(m_Destination).parent_path() /
                                       std::filesystem::u8path(file).filename())
                                          .generic_u8string();
            }
        }
        if (!m_SourceImport.empty())
        {
            PropertyUI::ReadOnly("import-source", "Source file", m_SourceImport.c_str());
            PropertyUI::Text("import-target", "New asset .hdr", m_SourceDestination);
            if (ImGui::Button("Copy validated HDR to Assets"))
                try
                {
                    auto source = std::filesystem::u8path(m_SourceImport),
                         destination = std::filesystem::u8path(m_SourceDestination);
                    AssetPath(destination, ".hdr");
                    auto bytes = ReadAssetBytes(source, 64 * 1024 * 1024);
                    (void)DecodeRadianceHDR(bytes);
                    auto target = Project::ResolveOwnedAsset(m_Assets->Root(), destination);
                    std::filesystem::create_directories(target.parent_path());
                    FileSystem::WriteFileAtomically(
                        target,
                        [&](auto &o) { o.write(bytes.data(), std::streamsize(bytes.size())); },
                        WriteMode::CreateNew);
                    m_CreateValues.Image = destination;
                    m_SourceImport.clear();
                    m_Assets->Refresh();
                    m_Picker = {};
                    if (AssetsChanged)
                        AssetsChanged();
                }
                catch (const std::exception &e)
                {
                    Error(e);
                }
        }
        ImGui::TextWrapped("Creates source/preparation intent. Prepare separately to publish "
                           "portable lighting; no GPU or script SDK is needed by the producer.");
        if (ImGui::Button("Create"))
        {
            auto path = std::filesystem::u8path(m_Destination);
            auto value = m_CreateValues;
            auto action = [this, path, value] { return Create(path, value); };
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
    auto title = std::string("Environment: ") + Name() + (Dirty() ? " *" : "") + "###Environment";
    ImGui::SetNextWindowSize({580, 600}, ImGuiCond_FirstUseEver);
    bool contents = ImGui::Begin(title.c_str(), &visible);
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!visible && RequestClose)
        RequestClose();
    if (contents)
    {
        bool busy = Busy && Busy();
        ImGui::BeginDisabled(!editable || (GuardPending && GuardPending()) || busy);
        if (ImGui::Button("Save"))
            Save();
        ImGui::SameLine();
        if (ImGui::Button("Reopen / discard") && ReplaceDocument)
        {
            auto path = m_Document->File().Path();
            ReplaceDocument([this, path] { return Open(path); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate..."))
            BeginCreate(m_Document->Reference().parent_path(), true);
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
        ImGui::BeginDisabled(Dirty());
        if (ImGui::Button("Prepare accepted lighting") && Prepare)
            Prepare(m_Document->Reference());
        ImGui::EndDisabled();
        if (Dirty())
            ImGui::TextDisabled("Save source/settings before Prepare.");
        ImGui::SameLine();
        if (ImGui::Button("Refresh status"))
        {
            m_Assets->Reload(m_Document->Reference());
            Inspect();
        }
        ImGui::EndDisabled();
        if (busy)
        {
            ImGui::TextWrapped(
                "Native preparation running; progress and diagnostics are in Console.");
            if (ImGui::Button("Cancel preparation") && Cancel)
                Cancel();
        }
        if (m_StatusEpoch != m_Assets->Epoch() || m_Status.empty())
            Inspect();
        ImGui::SeparatorText("Accepted resources / preparation status");
        ImGui::TextWrapped("%s", m_Status.c_str());
        PropertyUI::Validation(m_Error.c_str());
    }
    ImGui::End();
}
} // namespace Hazel
