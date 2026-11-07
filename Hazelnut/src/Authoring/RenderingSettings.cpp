#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/Renderer2D.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
#include <sstream>
#include "ScenePresentationCommand.h"
namespace Hazel {
    const std::string &AuthoringPanel::RenderingRestartReason() const {
        m_RenderingRestart.clear();
        if (auto project = Project::GetActive()) {
            try {
                m_RenderingRestart = RendererPolicy::RestartReason(project->GetRendererRequests(),
                                                                   Renderer::GetResolution(),
                                                                   Renderer::GetCapabilities());
            } catch (const std::exception &e) {
                m_RenderingRestart = e.what();
            }
        }
        return m_RenderingRestart;
    }
    void AuthoringPanel::ApplyRuntimeVSync() {
        if (auto project = Project::GetActive())
            Application::Get().GetWindow().SetVSync(project->GetRendererRequests().VSync);
    }
    void AuthoringPanel::RestoreEditorVSync() {
        Application::Get().GetWindow().SetVSync(m_Preferences.VSync);
    }
    bool AuthoringPanel::SaveRenderingRequests(const RuntimeRendererRequests &requests) {
        if (!Require(EditorAction::EditAsset))
            return false;
        try {
            RendererPolicy::Validate(requests);
            const auto project = Project::GetActive();
            auto candidate = CreateRef<Project>();
            candidate->GetConfig() = project->GetConfig();
            candidate->GetConfig().Rendering = requests;
            auto file = m_Editor.m_ProjectFile; // Staged ownership; failure never publishes config.
            file.Save(ProjectSerializer(candidate).SerializeText(),
                      Resources::Get().UserData / "recovery");
            project->GetConfig().Rendering = requests;
            m_Editor.m_ProjectFile = std::move(file);
            m_Editor.m_ProjectLoad.Saved();
            m_RenderingDraft = requests;
            m_RenderingAuthored = true;
            m_Editor.m_ActionError.clear();
            const auto reason = RenderingRestartReason();
            Notify(reason.empty() ? "Rendering requests saved; current GPU policy already meets "
                                    "them. Runtime VSync applies on Play/Simulate."
                                  : "Rendering requests saved. " + reason);
            return true;
        } catch (const std::exception &e) {
            return m_Editor.ActionFailed(e.what());
        }
    }
    bool AuthoringPanel::SaveNewSceneDefaults() {
        if(!Require(EditorAction::EditAsset))return false;
        try {
            const auto canonical=SceneDefaults::Read(SceneDefaults::Encode(m_SceneDefaultsDraft));
            auto project=Project::GetActive();auto candidate=CreateRef<Project>();candidate->GetConfig()=project->GetConfig();candidate->GetConfig().SceneDefaults=canonical;
            auto file=m_Editor.m_ProjectFile;file.Save(ProjectSerializer(candidate).SerializeText(),Resources::Get().UserData/"recovery");
            project->GetConfig().SceneDefaults=canonical;m_Editor.m_ProjectFile=std::move(file);m_Editor.m_ProjectLoad.Saved();
            Notify("New-scene defaults saved. Existing scenes and workspaces retain their authored values.");return true;
        }catch(const std::exception& error){return m_Editor.ActionFailed(error.what());}
    }
    void AuthoringPanel::SceneSettings() {
        if(!m_ShowSceneSettings || !m_Editor.m_EditorScene)return;
        ImGui::SetNextWindowSize({540,340},ImGuiCond_FirstUseEver);
        ImGui::Begin("Scene Settings",&m_ShowSceneSettings);
        auto& scene=*m_Editor.m_EditorScene;
        if(m_PresentationScene!=scene.GetIdentity()){m_PresentationScene=scene.GetIdentity();m_ScenePresentationDraft=scene.Presentation();}
        int mode = m_ScenePresentationDraft.SceneReferred ? 1 : 0;
        if (PropertyUI::Combo("presentation-mode", "World presentation", mode,
                              "Display-referred\0Scene-referred (Neutral)\0"))
        {
            m_ScenePresentationDraft.SceneReferred = mode == 1;
            m_ScenePresentationDraft.Exposure = 0;
        }
        if (mode)
            PropertyUI::SliderFloat("scene-exposure", "Exposure (stops)",
                                    m_ScenePresentationDraft.Exposure, -16, 16);
        PropertyUI::ReadOnly("presentation-source","Source","Authored scene; applies on Apply / Save");
        ImGui::TextWrapped(
            "Scene-referred world surfaces use exposure and fixed Neutral tone mapping. Screen UI "
            "stays display-linear and independent of exposure. Both camera projections are "
            "supported. Prepared image lighting is separate from the directional key.");
        PropertyUI::Color("scene-background","Background",&m_ScenePresentationDraft.Background.x,{},false);
        if(mode && Project::GetActive()) {
            ImGui::SeparatorText("Environment instance");
            auto &environment=m_ScenePresentationDraft.Environment;
            auto previous=environment.Asset;
            AssetPicker("environment","Environment asset",environment.Asset,".henv",Project::GetActive()->GetAssets(),m_EnvironmentPicker,[this](const auto &path){SelectAsset(path);});
            if(environment.Asset.empty() && !previous.empty())environment=SceneEnvironment{};
            if(!environment.Asset.empty()) {
                float degrees=glm::degrees(environment.Yaw);
                if(PropertyUI::SliderFloat("environment-yaw","Yaw (degrees)",degrees,-360,360))environment.Yaw=glm::radians(degrees);
                PropertyUI::DragFloat("environment-gain","Radiance gain",environment.Gain,.01f,0,1000);
                PropertyUI::Checkbox("environment-lighting","Image-based lighting",environment.Lighting);
                PropertyUI::Checkbox("environment-sky","Image background",environment.Background);
                auto status=Project::GetActive()->GetAssets()->Environment(environment.Asset);
                PropertyUI::ReadOnly("environment-effective","Effective generation",status.Accepted?(AssetKey(status.Accepted->Generation)+" / "+status.Accepted->Identity).c_str():"None; prepare the environment asset");
                PropertyUI::Validation(status.Error.c_str());
            }
            if(ImGui::Button("New environment..."))m_Environments.BeginCreate("Environments");
            ImGui::TextWrapped("Background and lighting share orientation/gain. Hiding the image background keeps the authored solid background. Yaw/gain need no rebake; source/quality edits do.");
        }
        if(!mode&&!m_ScenePresentationDraft.Environment.Asset.empty()&&ImGui::Button("Clear scene environment"))m_ScenePresentationDraft.Environment={};
        bool valid=true;try{m_ScenePresentationDraft.Validate();}catch(const std::exception &e){PropertyUI::Validation(e.what());valid=false;}
        const auto can=Availability(EditorAction::EditScene);ImGui::BeginDisabled(!can||!valid);
        if(ImGui::Button("Apply scene settings")) {
            PrepareSave();m_ActiveDocument=EditorDocument::Scene;
            if(!m_SceneHistory.Execute(scene,CreateScope<ScenePresentationCommand>(scene,m_ScenePresentationDraft)))m_Editor.ActionFailed(m_SceneHistory.TakeDiagnostic());
        }
        ImGui::EndDisabled();ImGui::SameLine();
        if(ImGui::Button("Revert form"))m_ScenePresentationDraft=scene.Presentation();
        ImGui::TextWrapped("Picker displays sRGB; scene stores linear RGB. Apply participates in Undo/Redo; Save writes the scene.");
        ImGui::End();
    }
    void AuthoringPanel::ProjectRendering() {
        if (!ImGui::CollapsingHeader("Runtime performance / advanced renderer requests", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::PushID("runtime-rendering");
        ImGui::TextWrapped("Portable requests for Play/Simulate and Nutella. GPU policies apply at "
                           "launch; no live renderer recreation.");
        PropertyUI::ReadOnly("request-source","Request source",Project::GetActive()->GetConfig().Rendering?"Authored project .hproj":"Built-in runtime defaults");
        PropertyUI::ReadOnly("effective-source","Effective source","Launch policy + detected context; batching/shaders require restart");
        const auto saved = Project::GetActive()->GetRendererRequests();
        const bool dirty = saved.VSync != m_RenderingDraft.VSync ||
                           saved.TextureSlots != m_RenderingDraft.TextureSlots ||
                           saved.ShaderLoading != m_RenderingDraft.ShaderLoading;
        ImGui::TextDisabled(dirty ? "Unsaved rendering draft"
                                  : "Saved project requests (or engine defaults)");
        const auto can = Availability(EditorAction::EditAsset);
        const bool resetVSync = true;
        if (PropertyUI::Checkbox(
                "vsync", "Runtime VSync", m_RenderingDraft.VSync, &resetVSync,
                {"Requests swap interval 1 or 0. Driver/compositor timing is not measured.",
                 nullptr, can.Reason}))
            m_RenderingAuthored = true;
        PropertyUI::ReadOnly("interval", "Current window interval",
                             Application::Get().GetWindow().IsVSync() ? "1 submitted"
                                                                      : "0 submitted");
        ImGui::TextWrapped(
            "Edit uses editor preference; Play/Simulate uses saved project request.");
        int shader = int(m_RenderingDraft.ShaderLoading),
            slots = int(m_RenderingDraft.TextureSlots);
        const int resetSlots = 32;
        if (PropertyUI::Combo(
                "shader", "Shader loading", shader, "Automatic\0GLSL compatibility\0",
                {"Auto chooses SPIR-V when available, otherwise shaderc/Cross to GLSL 410.",
                 nullptr, can.Reason})) {
            m_RenderingDraft.ShaderLoading = ShaderLoadingRequest(shader);
            m_RenderingAuthored = true;
        }
        const auto &caps = Renderer::GetCapabilities();
        const auto &current = Renderer::GetResolution();
        PropertyUI::ReadOnly("shader-available", "Available paths",
                             caps.ShaderBinaries ? "SPIR-V / GLSL 410" : "GLSL 410");
        const auto loaded = Renderer2D::GetQuadShaderLoadingPath();
        PropertyUI::ReadOnly("shader-effective", "Effective 2D program",
                             loaded == Shader::ProgramLoadingPath::SPIRV ? "SPIR-V specialization"
                             : loaded == Shader::ProgramLoadingPath::GeneratedGLSL
                                 ? "Generated GLSL 410"
                                 : "Legacy GLSL");
        if (ImGui::CollapsingHeader("Advanced batching")) {
            if (PropertyUI::DragInt(
                    "slots", "Texture batch slots", slots, 1, 2, 32, &resetSlots,
                    {"Includes white. A smaller limit can increase draw calls; no quality change.",
                     nullptr, can.Reason})) {
                m_RenderingDraft.TextureSlots = uint32_t(slots);
                m_RenderingAuthored = true;
            }
            PropertyUI::ReadOnly(
                "slot-available", "Available limit",
                ("Device " + std::to_string(caps.MaxTextureSlots) + "; engine 32").c_str());
            PropertyUI::ReadOnly("slot-effective", "Effective now",
                                 std::to_string(current.Effective.TextureSlots).c_str());
        }
        bool valid = true;
        try {
            const auto desired =
                RendererPolicy::Resolve(RendererPolicy::Settings(m_RenderingDraft), caps);
            if (!desired.TextureReason.empty())
                ImGui::TextWrapped("%s; launch limit %u. Authored request retained.",
                                   desired.TextureReason.c_str(), desired.Effective.TextureSlots);
            ImGui::TextWrapped("%s", desired.ShaderReason.c_str());
            const auto pending = RendererPolicy::RestartReason(m_RenderingDraft, current, caps);
            if (!pending.empty())
                ImGui::TextWrapped("%s. Save requests, save wanted documents, close normally, and "
                                   "launch this project again.",
                                   pending.c_str());
        } catch (const std::exception &e) {
            valid = false;
            PropertyUI::Validation(e.what());
        }
        ImGui::BeginDisabled(!valid || !can);
        if (ImGui::Button("Save rendering requests"))
            SaveRenderingRequests(m_RenderingDraft);
        ImGui::EndDisabled();
        PropertyUI::Help(can.Reason);
        ImGui::TextWrapped("This save preserves scene/asset drafts and other configuration. "
                           "General Project Settings still stages reopening when saved below.");
        if (ImGui::SmallButton("Detected device / editor graphics")) {
            m_ShowPreferences = true;
            m_PreferenceCategory = 2;
            m_PreferenceSearch.clear();
            m_Draft = m_Preferences;
        }
        ImGui::PopID();
    }
    void AuthoringPanel::EditorRendering() {
        PropertyUI::Checkbox("editor-vsync", "Editor VSync", m_Draft.VSync);
        PropertyUI::Help(
            "Live for the main editor window in Edit; restored after Stop. Detached windows follow "
            "their existing ImGui swap behavior. Timing is not measured.");
        PropertyUI::ReadOnly("swap-current", "Current submitted interval",
                             Application::Get().GetWindow().IsVSync() ? "1" : "0");
        if (m_Editor.m_SceneState != EditorLayer::SceneState::Edit)
            ImGui::TextWrapped("The current runtime uses the project's VSync request; editor "
                               "preference applies after Stop.");
        if (Project::GetActive() && ImGui::SmallButton("Project runtime rendering"))
            m_ShowProject = true;
        if (ImGui::CollapsingHeader("Advanced editor diagnostics")) {
            int debug = int(m_Draft.DebugOutput);
            if (PropertyUI::Combo(
                    "debug-request", "GL debug output", debug, "Automatic\0Off\0On\0",
                    {"Auto enables debug output in Debug builds, disables it in Release/Dist. "
                     "Captures driver messages; not the Console's capture threshold."}))
                m_Draft.DebugOutput = DebugOutputRequest(debug);
            const auto &current = Renderer::GetResolution();
            const auto &caps = Renderer::GetCapabilities();
            PropertyUI::ReadOnly("debug-available", "Available",
                                 caps.DebugOutput ? "Core debug functions supported"
                                                  : "Unavailable on this context");
            PropertyUI::ReadOnly("debug-effective", "Effective now",
                                 current.Effective.EnableDebugOutput ? "On" : "Off");
            const auto desired =
                RendererPolicy::Resolve(RendererPolicy::Settings({}, m_Draft.DebugOutput), caps);
            if (!desired.DebugReason.empty())
                ImGui::TextWrapped("%s", desired.DebugReason.c_str());
            if (desired.Effective.EnableDebugOutput != current.Effective.EnableDebugOutput)
                ImGui::TextWrapped("Save preferences and restart Hazelnut to change driver "
                                   "capture. Current callback remains unchanged.");
        }
    }
    void AuthoringPanel::CopyDeviceReport() {
        const auto &c = Renderer::GetCapabilities();
        std::ostringstream out;
        out << "OpenGL | " << c.Vendor << " | " << c.Device << " | " << c.Driver
            << "\nTexture slots/bindings/size: " << c.MaxTextureSlots << '/' << c.MaxTextureBindings
            << '/' << c.MaxTextureSize
            << "\nColor attachments/draw buffers: " << c.MaxColorAttachments << '/'
            << c.MaxDrawBuffers << "\nSample limits all/color/integer/depth: " << c.MaxSamples
            << '/' << c.MaxColorSamples << '/' << c.MaxIntegerSamples << '/' << c.MaxDepthSamples
            << "\nLine widths: " << c.MinLineWidth << "–" << c.MaxLineWidth
            << "\nShader binaries/debug functions: " << c.ShaderBinaries << '/' << c.DebugOutput
            << '\n'
            << RendererPolicy::Describe(Renderer::GetResolution())
            << "\nMain-window submitted swap interval: " << Application::Get().GetWindow().IsVSync()
            << " (timing not measured)";
        ImGui::SetClipboardText(out.str().c_str());
        Notify(out.str());
    }
} // namespace Hazel
