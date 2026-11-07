// Actual editor job/document owners plus ImGui widget activation. All files and
// profiles belong to the existing private editor fixture; no production hooks.
void ImportWorkflowUpdate()
{
    if(m_ImportPhase>=200){EnvironmentWorkflowUpdate();return;}
    auto &owner = *m_Editor.m_Authoring;
    auto assets = Project::GetActive()->GetAssets();
    if (m_ImportPhase == 101)
    {
        if (m_ImportWait)
        {
            --m_ImportWait;
            return;
        }
        Check(!owner.m_ShowSaveConflict, "Previous fixture conflict did not cancel");
        m_ImportPhase = 1;
    }
    if (m_ImportPhase == 1)
    {
        if (owner.m_ShowSaveConflict)
        {
            m_ImportPhase = 100;
            return;
        }
        m_ImportStarted = std::chrono::steady_clock::now();
        m_ImportPreferences = owner.m_Preferences;
        owner.m_Preferences.SDK = (m_Directory / "missing-import-sdk").generic_u8string();
        owner.m_Preferences.Python = (m_Directory / "missing-import-python").generic_u8string();
        owner.RefreshSDK();
        auto fixture = ImportFixture();
        m_NativeImportJSON = fixture.first;
        m_NativeImportBin = fixture.second;
        auto file = m_Directory / "native-editor.glb";
        FileSystem::WriteFileAtomically(file, [&](auto &out) {
            auto bytes = ImportGLB(fixture.first, fixture.second);
            out.write(bytes.data(), bytes.size());
        });
        Check(bool(m_Editor.m_ContentBrowserPanel->ImportGLB) &&
                  bool(m_Editor.m_ContentBrowserPanel->CreateMaterial),
              "Native import/material Content Browser commands missing");
        owner.BeginImport("Native", file);
        owner.m_ImportDestination = "Native/Imported/Diagnostic";
        Check(owner.m_ShowImport, "Native GLB import form did not open");
        m_ImportPhase = 2;
        return;
    }
    Check(std::chrono::steady_clock::now() - m_ImportStarted < std::chrono::seconds(30),
          "Native editor import workflow exceeded its bound");
    if (m_ImportWait)
    {
        --m_ImportWait;
        return;
    }
    if (m_ImportPhase == 3)
    {
        if (owner.m_Tools.Busy())
            return;
        Check(owner.m_Report.Success && !owner.m_Report.Request.NativeExecutable.empty(),
              "Native import form failed: " + owner.m_Report.Output);
        Check(!owner.m_Report.Python, "Native import incorrectly required Python");
        m_NativeImportManifest = "Native/Imported/Diagnostic/Import.himport";
        auto manifest = ReadImport(assets->Root() / m_NativeImportManifest);
        m_NativeImportGeneration = manifest.Generation;
        owner.SelectAsset(assets->Root() / m_NativeImportManifest);
        Check(owner.m_ShowMesh, "Accepted import did not expose native metadata");
        m_ImportPhase = 30;
        m_ImportWait = 2;
        return;
    }
    if (m_ImportPhase == 30)
    {
        auto available = owner.Availability(EditorAction::EditAsset);
        Check(bool(available), std::string("Native material creation unavailable: ") +
                                   (available.Reason ? available.Reason : ""));
        owner.m_Materials.BeginCreate("Native/Materials");
        Check(owner.m_Materials.m_Create, "Native material creation request was ignored");
        owner.m_Materials.m_CreateValues.Name = "Native authored material";
        m_ImportPhase = 4;
        return;
    }
    if (m_ImportPhase == 5)
    {
        Check(owner.m_Materials.HasDocument() &&
                  owner.m_Materials.Name() == "Native/Materials/NewMaterial.hmat",
              "ImGui material Create did not publish the native document: " +
                  owner.m_Materials.m_Error + " / " + owner.m_Materials.Name() +
                  " pending=" + std::to_string(owner.m_Documents.Pending()));
        m_ImportPhase = 6;
        return;
    }
    if (m_ImportPhase == 7)
    {
        Check(owner.m_Materials.m_Document->Draft().DoubleSided && owner.m_Materials.Dirty() &&
                  owner.m_Materials.CanUndo(),
              "ImGui material property did not enter document history");
        m_ImportPhase = 8;
        return;
    }
    if (m_ImportPhase == 9)
    {
        Check(!owner.m_Materials.Dirty() &&
                  ReadMaterial(assets->Root() / owner.m_Materials.Name()).DoubleSided,
              "ImGui material Save did not persist authored values");
        owner.m_ActiveDocument = EditorDocument::Material;
        Check(owner.UndoRedo(false) && owner.m_Materials.Dirty() &&
                  !owner.m_Materials.m_Document->Draft().DoubleSided,
              "Material undo after Save failed");
        Check(owner.UndoRedo(true) && !owner.m_Materials.Dirty(),
              "Material redo lost exact saved state");
        m_ImportPhase = 91;
        return;
    }
    if (m_ImportPhase == 93)
    {
        auto &document = *owner.m_Materials.m_Document;
        Check(!document.Draft().Textures[0].Image.empty() && document.Dirty() && document.CanUndo(),
              "Native named texture selector did not update material history");
        auto image = document.Draft().Textures[0].Image;
        Check(owner.m_Materials.UndoRedo(false) && document.Draft().Textures[0].Image.empty(),
              "Texture assignment undo failed");
        Check(owner.m_Materials.UndoRedo(true) && document.Draft().Textures[0].Image == image,
              "Texture assignment redo failed");
        Check(owner.m_Materials.Save(), "Assigned material texture did not save");
        m_ImportPhase = 10;
        return;
    }
    if (m_ImportPhase == 11)
    {
        Check(ImGui::FindWindowByName("Create material") &&
                  ImGui::FindWindowByName("Create material")->Active,
              "Material Duplicate did not open the native creation form");
        m_ImportPhase = 12;
        return;
    }
    if (m_ImportPhase == 13)
    {
        Check(owner.m_Materials.Name() == "Native/Materials/MaterialCopy.hmat" &&
                  owner.m_Materials.m_Document->Draft().DoubleSided,
              "Native material duplicate lost authored values");
        auto manifest = ReadImport(assets->Root() / m_NativeImportManifest);
        owner.m_Materials.Close();
        owner.m_ActiveDocument = EditorDocument::Scene;
        owner.InstantiatePrefab(assets->Root() / manifest.Prefab);
        auto scene = m_Editor.m_EditorScene;
        Check(scene->GetAllEntitiesWith<MeshRendererComponent>().size() == 2,
              "Native detached prefab instantiation lost mesh instances");
        auto handle = *scene->GetAllEntitiesWith<MeshRendererComponent>().begin();
        Entity mesh(handle, scene.get());
        m_Editor.m_SceneHierarchyPanel.SetSelectedEntity(mesh);
        auto before = UIEditValues::Capture(mesh), proposed = before;
        auto definition =
            ReadMeshAsset(assets->Root(), mesh.GetComponent<MeshRendererComponent>().Mesh);
        auto slot = definition.Definition.Slots[0].ID;
        proposed.Mesh->Materials[slot] = "Native/Materials/MaterialCopy.hmat";
        UIInspectorEditing inspector;
        PropertyUI::EditActivity edit;
        edit.Changed = edit.Committed = true;
        Check(inspector.Edit(*scene, &owner.m_SceneHistory, mesh.GetUUID(), "Mesh slot assignment",
                             before, proposed, edit, 0),
              "Mesh slot assignment failed shared property history");
        auto assigned = mesh.GetComponent<MeshRendererComponent>();
        auto initialTransform = mesh.GetComponent<TransformComponent>();
        before = UIEditValues::Capture(mesh);
        proposed = before;
        proposed.Transform->Translation.x += .25f;
        Check(inspector.Edit(*scene, &owner.m_SceneHistory, mesh.GetUUID(),
                             "Imported node transform", before, proposed, edit, 0),
              "Imported node property edit failed");
        Check(owner.UndoRedo(false) &&
                  mesh.GetComponent<TransformComponent>().Translation ==
                      initialTransform.Translation &&
                  mesh.GetComponent<MeshRendererComponent>().Materials == assigned.Materials,
              "Transform undo changed mesh slot assignment");
        Check(owner.UndoRedo(false) && mesh.GetComponent<MeshRendererComponent>().Materials.empty(),
              "Slot assignment undo failed");
        Check(owner.UndoRedo(true) && owner.UndoRedo(true),
              "Interleaved mesh/transform redo failed");
        auto copied = scene->DuplicateEntity(mesh);
        Check(copied.GetComponent<MeshRendererComponent>().Materials == assigned.Materials,
              "Duplicate lost mesh material overrides");
        scene->DestroyEntity(copied);
        auto authored = SceneSerializer(scene).CaptureAuthoredState();
        auto file = assets->Root() / "Scenes/NativeImported.hazel";
        Check(m_Editor.SerializeScene(scene, file) && m_Editor.OpenScene(file),
              "Imported native authoring Save/Reopen failed");
        Check(SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() == authored,
              "Imported Save/Reopen changed exact authored state");
        Check(!m_Editor.OnScenePlay(true) &&
                  m_Editor.m_ActionError.find("Scene-referred") != std::string::npos,
              "Lit imported scene bypassed scene-referred validation");
        owner.m_ShowSceneSettings = true;
        owner.m_PresentationScene = 0;
        m_ImportPhase = 130;
        m_ImportWait = 2;
        return;
    }
    if (m_ImportPhase == 130)
    {
        owner.m_ScenePresentationDraft.SceneReferred = true;
        owner.m_ScenePresentationDraft.Exposure = 0;
        m_ImportPhase = 131;
        return;
    }
    if (m_ImportPhase == 132)
    {
        auto scene = m_Editor.m_EditorScene;
        Check(scene->Presentation().SceneReferred,
              "Native Scene Settings did not apply scene-referred presentation");
        Check(owner.UndoRedo(false) && !scene->Presentation().SceneReferred &&
                  owner.UndoRedo(true) && scene->Presentation().SceneReferred,
              "Scene presentation Undo/Redo failed");
        auto &panel = m_Editor.m_SceneHierarchyPanel;
        Check(panel.AddEntity("Native directional key"), "Native light entity creation failed");
        auto light = panel.GetSelectedEntity();
        Check(panel.AddAuthoredComponent<DirectionalLightComponent>(light.GetUUID()),
              "Native light structural history failed");
        light = panel.GetSelectedEntity();
        auto before = UIEditValues::Capture(light), proposed = before;
        proposed.Light->Intensity = 3;
        UIInspectorEditing inspector;
        PropertyUI::EditActivity edit;
        edit.Changed = edit.Committed = true;
        Check(inspector.Edit(*scene, &owner.m_SceneHistory, light.GetUUID(),
                             "Directional intensity", before, proposed, edit, 0),
              "Native light property history failed");
        Check(owner.UndoRedo(false) &&
                  light.GetComponent<DirectionalLightComponent>().Intensity == 1 &&
                  owner.UndoRedo(true) &&
                  light.GetComponent<DirectionalLightComponent>().Intensity == 3,
              "Light property Undo/Redo failed");
        auto state = SceneSerializer(scene).CaptureAuthoredState();
        Check(m_Editor.SaveScene() && m_Editor.OpenScene(m_Editor.m_EditorScenePath) &&
                  SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() == state,
              "Native direct settings save/reopen failed");
        Check(m_Editor.OnScenePlay(true), "Direct imported content failed Play readiness");
        m_Editor.OnSceneStop();
        Check(SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() == state,
              "Direct Play/Stop changed authored content");
        owner.m_ShowSceneSettings = false;
        m_NativeImportAuthored = SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState();
        auto manifest = ReadImport(assets->Root() / m_NativeImportManifest);
        float two = 2;
        uint32_t bits;
        std::memcpy(&bits, &two, 4);
        for (int i = 0; i < 4; ++i)
            m_NativeImportBin[12 + i] = char(bits >> (8 * i));
        auto bytes = ImportGLB(m_NativeImportJSON, m_NativeImportBin);
        FileSystem::WriteFileAtomically(assets->Root() / manifest.Source,
                                        [&](auto &out) { out.write(bytes.data(), bytes.size()); });
        Check(owner.StartImport(true, m_NativeImportManifest),
              "Native explicit reimport could not start");
        m_ImportPhase = 14;
        return;
    }
    if (m_ImportPhase == 14)
    {
        if (owner.m_Tools.Busy())
            return;
        Check(owner.m_Report.Success, "Native reimport failed: " + owner.m_Report.Output);
        auto manifest = ReadImport(assets->Root() / m_NativeImportManifest);
        Check(manifest.Generation != m_NativeImportGeneration &&
                  SceneSerializer(m_Editor.m_EditorScene).CaptureAuthoredState() ==
                      m_NativeImportAuthored,
              "Reimport changed authored scene or failed generation acceptance");
        m_NativeAcceptedManifest = Read(assets->Root() / m_NativeImportManifest);
        auto source = m_NativeImportJSON;
        auto at = source.find("\"metallicFactor\":0");
        source.replace(at, std::strlen("\"metallicFactor\":0"), "\"metallicFactor\":0.7");
        auto bytes = ImportGLB(source, m_NativeImportBin);
        FileSystem::WriteFileAtomically(assets->Root() / manifest.Source,
                                        [&](auto &out) { out.write(bytes.data(), bytes.size()); });
        Check(owner.StartImport(true, m_NativeImportManifest),
              "Native conflicting reimport could not start");
        m_ImportPhase = 15;
        return;
    }
    if (m_ImportPhase == 15)
    {
        if (owner.m_Tools.Busy())
            return;
        Check(!owner.m_Report.Success &&
                  owner.m_ImportError.find("Reimport conflict") != std::string::npos &&
                  Read(assets->Root() / m_NativeImportManifest) == m_NativeAcceptedManifest,
              "Native conflict feedback lost accepted generation");
        Check(owner.m_Metadata && owner.m_MetadataAccepted,
              "Native metadata view did not pin accepted CPU resources");
        FileSystem::WriteFileAtomically(assets->Root() / m_NativeImportManifest,
                                        [&](auto &out) { out << "corrupt candidate manifest"; });
        assets->Reload(m_NativeImportManifest);
        m_ImportPhase = 16;
        m_ImportWait = 2;
        return;
    }
    if (m_ImportPhase == 16)
    {
        Check(!owner.m_MetadataError.empty() && owner.m_Metadata && owner.m_MetadataAccepted &&
                  owner.m_MetadataAccepted->Generation == owner.m_Metadata->Generation,
              "Corrupt metadata candidate retired the accepted native view");
        FileSystem::WriteFileAtomically(assets->Root() / m_NativeImportManifest,
                                        [&](auto &out) { out << m_NativeAcceptedManifest; });
        m_ImportPhase = 17;
        m_ImportWait = 2;
        return;
    }
    if (m_ImportPhase == 17)
    {
        Check(!owner.m_MetadataError.empty(),
              "Metadata retried filesystem reads without an explicit epoch boundary");
        assets->Reload(m_NativeImportManifest);
        m_ImportPhase = 18;
        m_ImportWait = 2;
        return;
    }
    if (m_ImportPhase == 18)
    {
        Check(owner.m_MetadataError.empty() && owner.m_MetadataAccepted,
              "Explicit metadata reload did not accept the repaired file");
        owner.m_ShowImport = owner.m_ShowMesh = false;
        owner.m_ImportError.clear();
        owner.m_Preferences = m_ImportPreferences;
        owner.RefreshSDK();
        m_Editor.m_ActionError.clear();
        Check(glGetError() == GL_NO_ERROR, "Native import/material UI produced OpenGL errors");
        std::cout
            << "PASS: native editor GLB form/job without Python/SDK, ImGui material "
               "create/property/texture assignment/save/duplicate, independent material history, "
               "mesh-slot/transform history interleaving, prefab instantiate/save/reopen, "
               "explicit presentation readiness, native Scene Settings Apply, "
               "light/history/save/reopen/Play, native reimport/conflict feedback and retained "
               "metadata "
               "reload\n";
        m_ImportPhase=200;
    }
}
void DrawImportWorkflow()
{
    if(m_ImportPhase>=200){DrawEnvironmentWorkflow();return;}
    auto &owner = *m_Editor.m_Authoring;
    auto activate = [&](const char *windowName, const char *button) {
        auto window = ImGui::FindWindowByName(windowName);
        Check(window && window->Active, std::string("Native authoring window missing at phase ") +
                                            std::to_string(m_ImportPhase) + ": " + windowName);
        if (window->Hidden || window->SkipItems)
            return false;
        ImGui::FocusWindow(window);
        ImGui::ActivateItem(window->GetID(button));
        m_ImportWait = 2;
        return true;
    };
    if (m_ImportPhase == 131)
    {
        if (activate("Scene Settings", "Apply scene settings"))
            m_ImportPhase = 132;
    }
    else if (m_ImportPhase == 100)
    {
        if (activate("Document changed externally", "Cancel"))
            m_ImportPhase = 101;
    }
    else if (m_ImportPhase == 2)
    {
        if (activate("Import GLB", "Import"))
            m_ImportPhase = 3;
    }
    else if (m_ImportPhase == 4)
    {
        if (activate("Create material", "Create"))
            m_ImportPhase = 5;
    }
    else if (m_ImportPhase == 6)
    {
        auto window =
            ImGui::FindWindowByName("Material: Native/Materials/NewMaterial.hmat###Material");
        Check(window && window->Active, "Native material panel missing");
        ImGui::FocusWindow(window);
        auto row = ImHashStr("sided", 0, window->ID);
        auto table = ImHashStr("##property", 0, row);
        ImGui::ActivateItem(ImHashStr("##value", 0, table));
        m_ImportWait = 2;
        m_ImportPhase = 7;
    }
    else if (m_ImportPhase == 8)
    {
        if (activate("Material: Native/Materials/NewMaterial.hmat###Material", "Save"))
            m_ImportPhase = 9;
    }
    else if (m_ImportPhase == 91)
    {
        auto *window =
            ImGui::FindWindowByName("Material: Native/Materials/NewMaterial.hmat###Material");
        Check(window && window->Active, "Native material selector owner missing");
        ImGui::FocusWindow(window);
        int slot = 0;
        auto seed = ImHashData(&slot, sizeof(slot), window->ID);
        auto row = ImHashStr("image", 0, seed);
        auto table = ImHashStr("##property", 0, row);
        ImGui::ActivateItem(ImHashStr("##value", 0, table));
        m_ImportWait = 2;
        m_ImportPhase = 92;
    }
    else if (m_ImportPhase == 92 && !m_ImportWait)
    {
        auto &choices = owner.m_Materials.m_Pickers[0].Choices;
        Check(!choices.empty(), "Named texture selector did not inventory images");
        auto &popup = ImGui::GetCurrentContext()->OpenPopupStack;
        Check(!popup.empty() && popup.back().Window, "Native texture selector popup missing");
        auto *window = popup.back().Window;
        if (window->Hidden || window->SkipItems)
            return;
        const auto &choice = choices.front();
        auto seed = ImHashStr(choice.Path.generic_u8string().c_str(), 0, window->ID);
        ImGui::FocusWindow(window);
        ImGui::ActivateItem(ImHashStr(choice.Label.c_str(), 0, seed));
        m_ImportWait = 2;
        m_ImportPhase = 93;
    }
    else if (m_ImportPhase == 10)
    {
        if (activate("Material: Native/Materials/NewMaterial.hmat###Material", "Duplicate..."))
            m_ImportPhase = 11;
    }
    else if (m_ImportPhase == 12)
    {
        if (activate("Create material", "Create"))
            m_ImportPhase = 13;
    }
    // Keep sampling/default metadata visible without a fake mesh preview.
}
