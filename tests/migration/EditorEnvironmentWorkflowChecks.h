// Continues the existing native authoring fixture with its actual document/job
// owners and ImGui widget activation. No Python authoring or desktop service.
void EnvironmentWorkflowUpdate()
{
    auto &a = *m_Editor.m_Authoring;
    auto assets = Project::GetActive()->GetAssets();
    if (m_ImportPhase == 200)
    {
        m_ImportStarted = std::chrono::steady_clock::now();
        m_ImportPreferences = a.m_Preferences;
        a.m_Preferences.SDK = (m_Directory / "missing-environment-sdk").generic_u8string();
        a.m_Preferences.Python = (m_Directory / "missing-environment-python").generic_u8string();
        a.RefreshSDK();
        auto bytes =
            FileDocument::Read(Resources::Resolve("Templates/Native/Environment_v1/Neutral.hdr"));
        FileSystem::WriteFileAtomically(
            assets->Root() / "Native/EnvironmentSource.hdr",
            [&](auto &o) { o.write(bytes.data(), std::streamsize(bytes.size())); });
        assets->Refresh();
        Check(bool(m_Editor.m_ContentBrowserPanel->CreateEnvironment),
              "Native environment Content Browser action missing");
        m_Editor.m_ContentBrowserPanel->CreateEnvironment("Native/Environments");
        a.m_Environments.m_CreateValues.Profile = {16, 16, 8, 128};
        m_ImportPhase = 201;
        m_ImportWait = 2;
        return;
    }
    Check(std::chrono::steady_clock::now() - m_ImportStarted < std::chrono::seconds(30),
          "Native environment authoring workflow exceeded its bound");
    if (m_ImportWait)
    {
        --m_ImportWait;
        return;
    }
    auto &panel = a.m_Environments;
    if (m_ImportPhase == 204)
    {
        Check(panel.HasDocument() &&
                  panel.m_Document->Draft().Image == "Native/EnvironmentSource.hdr",
              "Native environment Create/source selector failed: " + panel.m_Error);
        m_ImportPhase = 205;
        return;
    }
    if (m_ImportPhase == 208)
    {
        Check(panel.Dirty() && panel.CanUndo() && panel.m_Document->Draft().Profile.Samples == 256,
              "Native environment quality edit omitted file history");
        m_ImportPhase = 209;
        return;
    }
    if (m_ImportPhase == 210)
    {
        Check(!panel.Dirty() &&
                  ReadEnvironment(assets->Root() / panel.Name()).Profile.Samples == 256,
              "Native environment Save failed");
        a.m_ActiveDocument = EditorDocument::Environment;
        Check(a.UndoRedo(false) && panel.Dirty() &&
                  panel.m_Document->Draft().Profile.Samples == 128,
              "Environment undo after Save");
        Check(a.UndoRedo(true) && !panel.Dirty(), "Environment redo exact saved state");
        m_ImportPhase = 211;
        return;
    }
    if (m_ImportPhase == 212)
    {
        if (a.m_Tools.Busy())
            return;
        Check(a.m_Report.Success && !a.m_Report.Python,
              "Native environment preparation failed or used Python: " + a.m_Report.Output);
        auto accepted = ReadPreparedEnvironment(assets->Root(), panel.Name());
        m_NativeEnvironmentAcceptance = Read(assets->Root() / EnvironmentAcceptance(panel.Name()));
        Check(accepted.Specular.Mips.size() == 5 && !panel.Dirty(),
              "Preparation failed complete mips or dirtied authored history");
        ImGui::SetWindowPos("Scene Settings", {20, 20}, ImGuiCond_Always);
        ImGui::SetWindowSize("Scene Settings", {760, 650}, ImGuiCond_Always);
        const auto settings = m_Editor.m_EditorScene->Presentation();
        a.m_ShowSceneSettings = true;
        a.m_PresentationScene = m_Editor.m_EditorScene->GetIdentity();
        a.m_ScenePresentationDraft = settings;
        m_ImportPhase = 213;
        return;
    }
    if (m_ImportPhase == 215)
    {
        Check(a.m_ScenePresentationDraft.Environment.Asset ==
                  "Native/Environments/NewEnvironment.henv",
              "Native scene environment named selector failed");
        a.m_ScenePresentationDraft.Environment.Yaw = glm::radians(90.f);
        a.m_ScenePresentationDraft.Environment.Gain = .75f;
        m_ImportPhase = 216;
        return;
    }
    if (m_ImportPhase == 217)
    {
        auto scene = m_Editor.m_EditorScene;
        auto instance = scene->Presentation().Environment;
        Check(instance.Asset == "Native/Environments/NewEnvironment.henv" && instance.Gain == .75f,
              "Native scene environment Apply did not author values");
        a.m_ActiveDocument = EditorDocument::Scene;
        Check(a.UndoRedo(false) && scene->Presentation().Environment.Asset.empty() &&
                  a.UndoRedo(true) && scene->Presentation().Environment == instance,
              "Scene environment Undo/Redo");
        auto state = SceneSerializer(scene).CaptureAuthoredState();
        Check(m_Editor.SaveScene() && m_Editor.OpenScene(m_Editor.m_EditorScenePath) &&
                  SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() == state,
              "Scene environment save/reopen");
        auto source = assets->Root() / "Native/EnvironmentSource.hdr",
             offline = assets->Root() / "Native/EnvironmentSource.offline";
        std::filesystem::rename(source, offline);
        Check(m_Editor.OnScenePlay(true),
              "Accepted lighting required HDR source or script SDK for Play");
        m_Editor.OnSceneStop();
        std::filesystem::rename(offline, source);
        Check(SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() == state,
              "Environment Play/Stop changed authored settings");
        a.m_ShowSceneSettings = false;
        FileSystem::WriteFileAtomically(source, [&](auto &o) { o << "malformed HDR candidate"; });
        m_ImportPhase = 218;
        return;
    }
    if (m_ImportPhase == 219)
    {
        if (a.m_Tools.Busy())
            return;
        Check(!a.m_Report.Success && a.m_Report.Output.find("Radiance") != std::string::npos &&
                  Read(assets->Root() / EnvironmentAcceptance(panel.Name())) ==
                      m_NativeEnvironmentAcceptance,
              "Native failed preparation replaced acceptance or hid diagnostics");
        auto bytes =
            FileDocument::Read(Resources::Resolve("Templates/Native/Environment_v1/Neutral.hdr"));
        FileSystem::WriteFileAtomically(
            assets->Root() / "Native/EnvironmentSource.hdr",
            [&](auto &o) { o.write(bytes.data(), std::streamsize(bytes.size())); });
        // Cancellation before process launch is deterministic and uses the same
        // owned native job API as the visible Cancel preparation button.
        a.PrepareEnvironment(std::filesystem::u8path(panel.Name()));
        Check(a.m_Tools.Busy(), "Native preparation cancellation job missing");
        a.m_Tools.Cancel();
        m_ImportPhase = 220;
        return;
    }
    if (m_ImportPhase == 220)
    {
        if (a.m_Tools.Busy())
            return;
        Check(a.m_Report.Outcome == ToolOutcome::Cancelled &&
                  Read(assets->Root() / EnvironmentAcceptance(panel.Name())) ==
                      m_NativeEnvironmentAcceptance,
              "Native cancelled job changed accepted generation");
        auto reference = std::filesystem::u8path(panel.Name());
        panel.Close();
        Check(panel.Open(reference) && !panel.Dirty(), "Environment native Reopen failed");
        panel.Close();
        a.m_ActiveDocument = EditorDocument::Scene;
        a.m_Preferences = m_ImportPreferences;
        a.RefreshSDK();
        m_Editor.m_ActionError.clear();
        Check(glGetError() == GL_NO_ERROR, "Environment native UI OpenGL errors");
        std::cout
            << "PASS: native environment ImGui create/HDR selector/quality/save, file undo/redo, "
               "SDK-free prepare job, scene named selection/Apply/history/save/reopen/Play with "
               "HDR unavailable, failure/cancel retention\n";
        m_Done = true;
        Application::Get().Close();
    }
}
void DrawEnvironmentWorkflow()
{
    auto &a = *m_Editor.m_Authoring;
    auto activate = [&](const char *windowName, const char *button) {
        auto w = ImGui::FindWindowByName(windowName);
        Check(w && w->Active, std::string("Environment UI owner missing at phase ") +
                                  std::to_string(m_ImportPhase) + ": " + windowName);
        if (w->Hidden || w->SkipItems)
            return false;
        ImGui::FocusWindow(w);
        ImGui::ActivateItem(w->GetID(button));
        m_ImportWait = 2;
        return true;
    };
    auto picker = [&](const char *windowName, const char *key) {
        auto w = ImGui::FindWindowByName(windowName);
        Check(w && w->Active, "Environment named selector owner missing");
        if (w->Hidden || w->SkipItems)
            return false;
        auto row = ImHashStr(key, 0, w->ID), table = ImHashStr("##property", 0, row);
        ImGui::FocusWindow(w);
        ImGui::ActivateItem(ImHashStr("##value", 0, table));
        m_ImportWait = 2;
        return true;
    };
    auto select = [&](const std::vector<AssetPickerState::Choice> &choices,
                      const std::filesystem::path &path) {
        auto &stack = ImGui::GetCurrentContext()->OpenPopupStack;
        Check(!stack.empty() && stack.back().Window, "Environment named selector popup missing");
        auto w = stack.back().Window;
        if (w->Hidden || w->SkipItems)
            return false;
        auto it =
            std::find_if(choices.begin(), choices.end(), [&](auto &c) { return c.Path == path; });
        Check(it != choices.end(), "Environment selector omitted owned asset");
        auto seed = ImHashStr(it->Path.generic_u8string().c_str(), 0, w->ID);
        ImGui::FocusWindow(w);
        ImGui::ActivateItem(ImHashStr(it->Label.c_str(), 0, seed));
        m_ImportWait = 2;
        return true;
    };
    const char *window = "Environment: Native/Environments/NewEnvironment.henv###Environment";
    if (m_ImportPhase == 201)
    {
        if (picker("Create environment", "source"))
            m_ImportPhase = 202;
    }
    else if (m_ImportPhase == 202 && !m_ImportWait)
    {
        if (select(a.m_Environments.m_Picker.Choices, "Native/EnvironmentSource.hdr"))
            m_ImportPhase = 203;
    }
    else if (m_ImportPhase == 203 && !m_ImportWait)
    {
        if (activate("Create environment", "Create"))
            m_ImportPhase = 204;
    }
    else if (m_ImportPhase == 205)
    {
        if (activate(window, "Preparation quality"))
            m_ImportPhase = 206;
    }
    else if (m_ImportPhase == 206 && !m_ImportWait)
    {
        auto w = ImGui::FindWindowByName(window);
        auto tree = ImHashStr("Preparation quality", 0, w->ID), row = ImHashStr("samples", 0, tree),
             table = ImHashStr("##property", 0, row);
        ImGui::FocusWindow(w);
        ImGui::ActivateItem(ImHashStr("##value", 0, table));
        m_ImportWait = 2;
        m_ImportPhase = 207;
    }
    else if (m_ImportPhase == 207 && !m_ImportWait)
    {
        auto &stack = ImGui::GetCurrentContext()->OpenPopupStack;
        Check(!stack.empty() && stack.back().Window, "Environment quality combo missing");
        auto w = stack.back().Window;
        if (!w->Hidden && !w->SkipItems)
        {
            ImGui::FocusWindow(w);
            ImGui::ActivateItem(w->GetID("256"));
            m_ImportWait = 2;
            m_ImportPhase = 208;
        }
    }
    else if (m_ImportPhase == 209)
    {
        if (activate(window, "Save"))
            m_ImportPhase = 210;
    }
    else if (m_ImportPhase == 211)
    {
        if (activate(window, "Prepare accepted lighting"))
            m_ImportPhase = 212;
    }
    else if (m_ImportPhase == 213)
    {
        if (picker("Scene Settings", "environment"))
            m_ImportPhase = 214;
    }
    else if (m_ImportPhase == 214 && !m_ImportWait)
    {
        if (select(a.m_EnvironmentPicker.Choices, "Native/Environments/NewEnvironment.henv"))
            m_ImportPhase = 215;
    }
    else if (m_ImportPhase == 216)
    {
        if (activate("Scene Settings", "Apply scene settings"))
            m_ImportPhase = 217;
    }
    else if (m_ImportPhase == 218)
    {
        if (activate(window, "Prepare accepted lighting"))
            m_ImportPhase = 219;
    }
}
