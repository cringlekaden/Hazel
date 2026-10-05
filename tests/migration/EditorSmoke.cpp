#ifdef HZ_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsCommandLine.h"
#endif
// Real pinned EditorLayer/panels in Application; files and layout are isolated.
#include "EditorLayer.h"
#include "UI/PropertyUI.h"
#include "Hazel/Core/FileSystem.h"
#include "Authoring/EditorPreferences.h"
#include "Authoring/AuthoringPanel.h"
#include "Hazel/Utils/Toolchain.h"
#include "Hazel/Utils/Process.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Project/ScriptSource.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectSerializer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <random>
#include <stdexcept>
#ifdef HZ_PLATFORM_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef HZ_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace Hazel {
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static std::string Read(const std::filesystem::path& file) {
    std::ifstream input(file,std::ios::binary); return {std::istreambuf_iterator<char>(input),{}};
}
}
#include "EditorDocumentChecks.h"
namespace Hazel {
class EditorWorkflowSmoke : public Layer {
public:
    EditorWorkflowSmoke(EditorLayer& editor, std::filesystem::path directory, bool& done)
        : m_Editor(editor), m_Directory(std::move(directory)), m_Done(done) {}
    void OnUpdate(Timestep) override {
        auto& e = m_Editor;
        switch (++m_Frame) {
        case 3: {
            Check(e.m_ContentBrowserPanel && ScriptEngine::IsInitialized(), "Editor project/assembly startup failed");
            EditorDocumentChecks();
            AuthoringChecks();
            auto* viewport = ImGui::FindWindowByName("Viewport");
            Check(viewport && viewport->DockId && e.m_ViewportSize.x > 0 && e.m_ViewportSize.y > 0, "Docked editor viewport missing");
            auto oldScene=e.m_ActiveScene;
            auto stale=oldScene->CreateEntity("Retired selection"); e.m_HoveredEntity=stale;
            e.NewScene(); Check(e.m_EditorScene == e.m_ActiveScene, "NewScene did not replace editor scene");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(stale),"NewScene retained borrowed observations");
            oldScene.reset(); // The next selection arrives before another picking update, with a destroyed Scene pointer.
            Check(!e.m_SceneHierarchyPanel.SetSelectedEntity(stale),"Selection dereferenced a destroyed foreign scene");
            auto camera = e.m_EditorScene->CreateEntityWithUUID(900, "Camera");
            camera.AddComponent<CameraComponent>().Camera.SetOrthographic(4, -1, 1);
            camera.AddComponent<ScriptComponent>().ClassName = "Regression.Camera";
            auto player = e.m_EditorScene->CreateEntityWithUUID(901, u8"Player %n é");
            player.AddComponent<SpriteRendererComponent>();
            player.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
            player.AddComponent<BoxCollider2DComponent>();
            player.AddComponent<ScriptComponent>().ClassName = "Regression.Player";
            auto texture = m_Directory / "AuthoringProject/Assets/Textures" / std::filesystem::u8path(u8"texture é 🚀.png");
            std::filesystem::copy_file(m_Directory / "assets/textures/Checkerboard.png", texture);
            const auto payload = texture.lexically_relative(m_Directory).generic_u8string();
            const auto decoded = ContentBrowserPath(payload.c_str(), static_cast<int>(payload.size() + 1));
            Check(decoded == texture.lexically_relative(m_Directory), "Portable content-browser payload lost Unicode path");
            Check(SceneHierarchyPanel::AssignSpriteTexture(player.GetComponent<SpriteRendererComponent>(),decoded),"Drag/drop assignment code failed");
            auto& authored=ScriptEngine::GetScriptFieldMap(player)["Speed"];
            authored.Field={ScriptFieldType::Float,"Speed",nullptr}; authored.SetValue<float>(4.5f);
            auto external=e.m_EditorScene->CreateEntityWithUUID(902,"External texture");
            auto& externalSprite=external.AddComponent<SpriteRendererComponent>();
            Check(SceneHierarchyPanel::AssignSpriteTexture(externalSprite,m_Directory/"assets/textures/Checkerboard.png"),"External assignment failed");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.OnDuplicateEntity();
            auto duplicate = e.m_SceneHierarchyPanel.GetSelectedEntity();
            Check(duplicate && duplicate.GetUUID() != player.GetUUID(), "Editor duplication failed");
            e.m_EditorScene->DestroyEntity(duplicate);
            Check(!e.m_SceneHierarchyPanel.GetSelectedEntity(),"Destroyed selection remained valid");
            e.m_HoveredEntity=duplicate;
            Check(!e.m_HoveredEntity,"Destroyed hovered entity remained valid");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.m_EditorScenePath = m_Directory / "AuthoringProject/Assets/Scenes" / std::filesystem::u8path(u8"workflow-é-🚀.hazel");
            Check(e.SaveScene(),"Editor scene save reported failure");
            Project::GetActive()->GetConfig().StartScene=e.m_EditorScenePath.lexically_relative(std::filesystem::absolute(Project::GetAssetDirectory()));
            Check(e.SaveProject(),"Editor project save reported failure");
            const auto saved=Read(e.m_EditorScenePath);
            Check(saved.find(std::string(u8"Textures/texture é 🚀.png"))!=std::string::npos &&
                  saved.find("AuthoringProject/Assets/Textures")==std::string::npos,"Project texture was not asset-root relative");
            Check(saved.find((m_Directory/"assets/textures/Checkerboard.png").generic_u8string())!=std::string::npos,
                  "External absolute reference was discarded");
            break;
        }
        case 4: {
            auto file = e.m_EditorScenePath; e.NewScene(); Check(e.OpenScene(file),"OpenScene reported failure");
            auto player = e.m_EditorScene->GetEntityByUUID(901);
            Check(player && player.GetComponent<SpriteRendererComponent>().Resolved.Data->Texture->IsLoaded(), "Editor save/reopen/texture failed");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            KeyPressedEvent event(Key::W); e.OnKeyPressed(event);
            Check(e.m_GizmoType == -1, "Unfocused native key event bypassed ImGui shortcut ownership");
            e.m_ShowPhysicsColliders = true;
            // Original upstream Windows separators remain readable on Linux too.
            e.m_HoveredEntity=player;
            Check(e.OpenScene(m_Directory/"AuthoringProject/Assets/Scenes/Example.hazel"),"Legacy Windows-separated texture reference failed");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(player),"OpenScene retained retired observations");
            Check(e.OpenScene(file),"Authored scene reopen failed");
            FailureChecks();
            ImGui::SetWindowFocus("Viewport");
            // Deliver the fixture's paired release/press in one frame. Production
            // keeps ImGui's normal event trickling; there is no timing/click driver.
            m_Trickle=ImGui::GetIO().ConfigInputTrickleEventQueue;
            ImGui::GetIO().ConfigInputTrickleEventQueue=false;
            ImGui::GetIO().AddKeyEvent(ImGuiKey_W,true);
            break;
        }
        case 5: {
            KeyPressedEvent event(Key::E); e.OnKeyPressed(event);
            Check(e.m_GizmoType == ImGuizmo::TRANSLATE, "Native key event bypassed focused commands");
            ImGui::SetWindowFocus("Viewport");
            ImGui::GetIO().AddKeyEvent(ImGuiKey_W,false);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_E,true);
            break;
        }
        case 6: {
            KeyPressedEvent event(Key::R); e.OnKeyPressed(event);
            if(e.m_GizmoType!=ImGuizmo::ROTATE)
                std::cerr<<"Shortcut evidence: gizmo="<<e.m_GizmoType<<" viewport="<<e.m_ViewportFocused
                         <<" text="<<ImGui::GetIO().WantTextInput<<" active="<<ImGui::IsAnyItemActive()
                         <<" E="<<ImGui::IsKeyPressed(ImGuiKey_E,false)<<" ctrl="<<ImGui::GetIO().KeyCtrl
                         <<" popup="<<ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)
                         <<" gizmoUsing="<<ImGuizmo::IsUsing()<<"\n";
            Check(e.m_GizmoType == ImGuizmo::ROTATE, "Focused ImGui E did not select rotation");
            auto& io=ImGui::GetIO();
            e.m_GizmoType=ImGuizmo::SCALE;
            const bool textInput=io.WantTextInput;io.WantTextInput=true;
            e.m_Authoring->Shortcuts();
            Check(e.m_GizmoType==ImGuizmo::SCALE,"Text input allowed an editor shortcut");
            io.WantTextInput=textInput;
            ImGui::SetWindowFocus("Output");e.m_ViewportFocused=true;
            e.m_Authoring->Shortcuts();
            Check(e.m_GizmoType==ImGuizmo::SCALE,"Stale viewport observation bypassed current panel focus");
            ImGui::SetWindowFocus("Viewport");e.m_ViewportFocused=false;
            e.m_Authoring->Shortcuts();
            Check(e.m_GizmoType==ImGuizmo::ROTATE,"Current viewport focus was ignored");
            e.m_GizmoType=ImGuizmo::SCALE;e.m_ViewportFocused=true;
            io.ConfigInputTrickleEventQueue=m_Trickle;
            io.AddKeyEvent(ImGuiKey_E,false);
            auto authored=e.m_EditorScene->GetEntityByUUID(901); e.m_HoveredEntity=authored;
            e.OnScenePlay(); Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->IsRunning(), "Editor play failed");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(authored),"Play retained editor observations");
            e.OnScenePause();
            break;
        }
        case 7:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") == 0, "Paused editor advanced scripts");
            e.m_ActiveScene->Step(); break;
        case 8:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") > 0, "Editor single-step failed");
            {
                auto instance=ScriptEngine::GetEntityScriptInstance(901); instance->SetFieldValue<float>("Speed",88);
                Check(e.ReloadScripts(),"Editor valid reload failed");
                Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Speed")==88,"Editor reload lost live fields");
                Check(ScriptEngine::GetScriptFieldMap(e.m_EditorScene->GetEntityByUUID(901)).at("Speed").GetValue<float>()==4.5f,"Reload mutated editor defaults");
                FailureChecks();
                e.m_HoveredEntity=e.m_ActiveScene->GetEntityByUUID(901);
                Check(e.SaveScene(),"Save while playing failed"); e.OnSceneStop();
                Check(!e.m_HoveredEntity,"Stop retained runtime hover");
                e.OnScenePlay();
                Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Speed")==4.5f,"New Play inherited prior live fields");
                e.OnSceneStop();
            }
            Check(e.m_ActiveScene == e.m_EditorScene && !e.m_ActiveScene->IsRunning(), "Editor stop did not restore editor scene");
            e.m_HoveredEntity=e.m_EditorScene->GetEntityByUUID(901);
            {
                const auto saved=e.m_Authoring->m_SavedScene;
                e.m_EditorScene->GetEntityByUUID(901).GetComponent<TagComponent>().Tag="Unsaved simulation edit";
                e.OnSceneSimulate();
                Check(e.m_Authoring->m_SavedScene==saved && saved!=SceneSerializer(e.m_EditorScene).SerializeText(),"Simulation incorrectly marked unsaved authoring as saved");
            }
            Check(!e.m_HoveredEntity,"Simulate retained editor hover"); break;
        case 9:
            Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->GetEntityByUUID(901).GetComponent<Rigidbody2DComponent>().RuntimeBody, "Editor simulation failed");
            e.OnSceneStop(); e.OnScenePlay(); e.OnSceneStop();
            e.OnScenePlay();
            {
                auto retired=e.m_ActiveScene; auto instance=ScriptEngine::GetEntityScriptInstance(901);
                e.m_HoveredEntity=retired->GetEntityByUUID(901);
                Check(e.OpenProject(e.m_ProjectPath),"Editor project reopen reported failure");
                Check(!retired->IsRunning() && !instance->GetManagedObject() && !e.m_HoveredEntity,
                      "Project replacement retained runtime/borrowed owners");
            }
            Check(e.m_ContentBrowserPanel && ScriptEngine::IsInitialized(), "Editor project reopen failed");
            {
                const auto relocated=m_Directory/std::filesystem::u8path(u8"Relocated Project é 🚀");
                std::filesystem::copy(m_Directory/"AuthoringProject",relocated,std::filesystem::copy_options::recursive);
                const auto original=m_Directory/"AuthoringProject";
                const auto parked=m_Directory/"Parked original";
                // Windows cannot rename the watched project's ancestor directory while its watcher is active.
                // Switch to the copy (joining the old watcher), then make the original unavailable and reopen.
                Check(e.OpenProject(relocated/"Authoring.hproj"),"Copied project initial open failed");
                std::filesystem::rename(original,parked); // Prevent silently resolving against the original project.
                Check(e.OpenProject(relocated/"Authoring.hproj"),"Relocated project open failed");
                auto texture=e.m_EditorScene->GetEntityByUUID(901).GetComponent<SpriteRendererComponent>().Resolved.Data->Texture;
                // The asset cache canonicalizes resource paths. Windows temporary
                // directories may arrive as 8.3 aliases; compare file identity.
                const auto expected=relocated/"Assets/Textures"/std::filesystem::u8path(u8"texture é 🚀.png");
                Check(texture && texture->IsLoaded() && std::filesystem::equivalent(std::filesystem::u8path(texture->GetPath()),expected),"Relocation used original asset root");
                Check(e.m_EditorScene->GetEntityByUUID(902).GetComponent<SpriteRendererComponent>().Resolved.Data->Texture->IsLoaded(),"Relocation discarded external reference");
                std::filesystem::rename(parked,original);
            }
            break;
        case 10:
            Check(glGetError() == GL_NO_ERROR, "Editor/gizmo/panels OpenGL error");
            e.m_Authoring->SelectAsset(Project::GetAssetDirectory()/std::filesystem::u8path(u8"Prefabs/é independent.hprefab"));
            break;
        case 11: {
            auto* inspector=ImGui::FindWindowByName("Prefab Inspector");
            Check(inspector && inspector->Active,"Prefab asset did not open its ImGui inspector");
            e.m_Authoring->Guard(OperationIntent::ClosePrefab,[&e]{e.m_Authoring->ClosePrefab();return true;});
            Check(!e.m_Authoring->m_PrefabScene,"Clean prefab Close retained document");
            break;
        }
        case 14: {
            Check(!ImGui::FindWindowByName("Prefab Inspector")->Active,"Clean prefab Close did not release its inspector safely");
            const auto root=Project::GetAssetDirectory();
            SpriteSheetDocument document(Project::GetActive()->GetAssets());
            document.Create(std::filesystem::u8path(u8"Textures/texture é 🚀.png"),"Textures/inspector.hsprites");
            auto& sheet=document.Draft();
            sheet.Regions.push_back({sheet.NewID(),"Preview",{0,0,sheet.Sampling.Width,sheet.Sampling.Height},{.5f,1}});
            sheet.Clips.push_back({sheet.NewID(true),"Hold",true,{{sheet.Regions[0].ID,.2}}});
            document.Changed();document.Save();
            e.m_Authoring->SelectAsset(root/"Textures/inspector.hsprites");
            break;
        }
        case 15: {
            auto* panel=ImGui::FindWindowByName("Sprite Sheet: inspector.hsprites###Sprite Sheet");
            Check(panel && panel->Active,"Sprite asset did not open its dockable authoring panel");
            auto sheet=Project::GetActive()->GetAssets()->Sheet("Textures/inspector.hsprites");
            e.m_SceneHierarchyPanel.SetSelectedEntity(e.m_EditorScene->GetEntityByUUID(901));
            UsabilityChecks();
            e.m_Authoring->m_Sprites.AssignSprite({"Textures/inspector.hsprites",sheet->Regions[0].ID},e.m_Authoring->AssignmentTarget());
            e.m_Authoring->m_Sprites.AssignClip({"Textures/inspector.hsprites",sheet->Clips[0].ID},e.m_Authoring->AssignmentTarget());
            break;
        }
        case 16:
            Check(e.m_EditorScene->GetEntityByUUID(901).HasComponent<SpriteAnimationComponent>() && e.m_EditorScene->RenderedSprite(e.m_EditorScene->GetEntityByUUID(901)),"Sheet assignment did not resolve scene animation");
            Check(glGetError()==GL_NO_ERROR,"Sprite authoring panel OpenGL error");
            e.m_Authoring->m_Sprites.m_Selected=e.m_Authoring->m_Sprites.m_Document->Draft().Regions.front().ID;
            e.m_Authoring->m_Sprites.m_SelectedClip=e.m_Authoring->m_Sprites.m_Document->Draft().Clips.front().ID;
            Project::GetActive()->GetAssets()->Reload("Textures/inspector.hsprites");
            break;
        case 17: {
            // The visible authoring canvas must follow the same explicit cache
            // invalidation as scene resolution, even when sampling is unchanged.
            auto* sprite=e.m_EditorScene->RenderedSprite(e.m_EditorScene->GetEntityByUUID(901));
            Check(sprite && sprite->Texture,"Reloaded editor sprite missing");
            const auto texture=(ImTextureID)(uintptr_t)sprite->Texture->GetRendererID();
            bool visible=false;auto* draw=ImGui::GetDrawData();
            for(int list=0;draw && list<draw->CmdListsCount;++list)
                for(const auto& command:draw->CmdLists[list]->CmdBuffer)
                    if(command.TextureId==texture)visible=true;
            Check(visible,"Sprite preview retained a stale texture after asset reload");
            // Optional evidence from this bounded render smoke; no clicks or image assertions.
            if (const auto* capture=std::getenv("HAZEL_EDITOR_CAPTURE")) {
                auto* window=static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
                int width=0,height=0;glfwGetFramebufferSize(window,&width,&height);
                std::vector<unsigned char> pixels(static_cast<size_t>(width)*height*3);
                // Hidden native front buffers are not reliable screenshot targets.
                // Replay the already-produced draw data into an owned capture framebuffer.
                GLint buffer=0,alignment=0,readFramebuffer=0,drawFramebuffer=0,viewport[4]{};
                GLfloat clear[4]{};
                glGetIntegerv(GL_READ_BUFFER,&buffer);glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFramebuffer);
                glGetIntegerv(GL_VIEWPORT,viewport);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
                FramebufferSpecification specification;specification.Width=width;specification.Height=height;
                specification.Attachments={FramebufferTextureFormat::RGBA8};
                auto target=Framebuffer::Create(specification);target->Bind();
                glClearColor(.08f,.08f,.08f,1);glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,1);
                glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
                glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFramebuffer);
                glReadBuffer(buffer);glPixelStorei(GL_PACK_ALIGNMENT,alignment);
                glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
                glClearColor(clear[0],clear[1],clear[2],clear[3]);
                std::ofstream image(std::filesystem::u8path(capture),std::ios::binary);
                image<<"P6\n"<<width<<" "<<height<<"\n255\n";
                for(int y=height-1;y>=0;--y)image.write(reinterpret_cast<const char*>(pixels.data()+static_cast<size_t>(y)*width*3),width*3);
                Check(bool(image),"Editor screenshot write failed");
            }
            m_Done = true; Application::Get().Close();break;
        }
        }
    }
private:
    void OnImGuiRender() override {
        if(m_Frame!=13)return;
        ImGui::SetNextWindowSize({420,180});
        ImGui::Begin("Property layout contract",nullptr,ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing);
        {
            PropertyUI::Row row("first-appearance","Left label");
            Check(ImGui::GetContentRegionAvail().x>100,"Fresh property control column collapsed");
            float value=0;ImGui::InputFloat("##value",&value);
        }
        ImGui::End();
    }
    void UsabilityChecks() {
        auto& e=m_Editor;auto& a=*e.m_Authoring;auto& panel=a.m_Sprites;
        const auto target=a.AssignmentTarget();const auto scene=e.m_EditorScene;
        const auto before=SceneSerializer(scene).SerializeText();
        const auto sheetPath=Project::GetAssetDirectory()/"Textures/inspector.hsprites";
        const auto bytes=Read(sheetPath);auto& draft=panel.m_Document->Draft();
        const auto region=draft.Regions.front().ID;const auto width=draft.Regions.front().Rect.Width;
        draft.Regions.front().Rect.Width=0;panel.m_Document->Changed();
        Check(!panel.Assign(false,region)&&panel.Dirty()&&Read(sheetPath)==bytes&&SceneSerializer(scene).SerializeText()==before,"Failed Save Sheet and Assign mutated scene/draft/file");
        draft.Regions.front().Rect.Width=width;draft.Regions.front().Name="Accepted saved region";
        Check(panel.Assign(false,region)&&!panel.Dirty()&&std::get<SpriteReference>(scene->GetEntityByUUID(901).GetComponent<SpriteRendererComponent>().Source).Region==region,"Save Sheet and Assign did not save before applying identity");
        const auto after=SceneSerializer(scene).SerializeText();
        panel.AssignSprite({"Textures/inspector.hsprites",region},{target.Scene+1,target.Entity});
        panel.AssignSprite({"Textures/inspector.hsprites",region},{target.Scene,0});
        Check(SceneSerializer(scene).SerializeText()==after,"Stale/invalid assignment target mutated scene");
        draft.Regions.front().Name="Retained dirty region";panel.m_Document->Changed();
        const auto oldDoc=panel.m_Document.get();
        FileSystem::WriteFileAtomically(Project::GetAssetDirectory()/"Textures/future.hsprites",[](auto& out){out<<"SpriteSheetVersion: 99\n";});
        a.Guard(OperationIntent::OpenSheet,[&]{return panel.Open(Project::GetAssetDirectory()/"Textures/future.hsprites");});
        Check(a.m_Documents.Pending(),"Dirty sheet replacement skipped guard");
        a.m_ResolvingDocumentAction=true;
        Check(!a.m_Documents.Resolve(GuardChoice::Discard,a.Documents(),[&](const auto& doc){return a.SaveDocument(doc);}),"Invalid sheet Open succeeded");
        a.m_ResolvingDocumentAction=false;
        Check(panel.m_Document.get()==oldDoc&&panel.Dirty()&&draft.Regions.front().Name=="Retained dirty region"&&e.m_EditorScene==scene,"Failed Open discarded previous sheet/session draft");
        a.m_Documents.Resolve(GuardChoice::Cancel,a.Documents(),[&](const auto& doc){return a.SaveDocument(doc);});panel.m_Recovery=false;
        auto jobs=a.m_Tools.Start({m_Directory/"missing-python",{},e.m_ProjectPath,{},"Availability fixture"});
        Check(jobs&&a.m_Tools.Busy(),"Tool availability fixture did not start");
        const auto count=scene->GetAllEntitiesWith<IDComponent>().size();
        a.InstantiatePrefab(Project::GetAssetDirectory()/std::filesystem::u8path(u8"Prefabs/é independent.hprefab"));
        panel.AssignClip({"Textures/inspector.hsprites",draft.Clips.front().ID},target);
        a.CreatePrefab(scene->GetEntityByUUID(901));
        Check(scene->GetAllEntitiesWith<IDComponent>().size()==count&&!scene->GetEntityByUUID(901).HasComponent<SpriteAnimationComponent>()&&!a.m_CreatePrefab,"Tool job allowed callback/drag/drop mutation");
        Check(!e.SaveScene()&&!panel.Save()&&panel.Dirty(),"Tool job allowed protected input saves");
        ToolReport report;bool joined=false;
        for(int attempt=0;attempt<200;++attempt){if(a.m_Tools.Poll(report)){joined=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        Check(joined&&!a.m_Tools.Busy(),"Tool availability fixture failed to join");
        a.m_ActiveDocument=EditorDocument::Sheet;
        Check(a.SaveActive()&&!panel.Dirty()&&draft.Regions.front().Name=="Retained dirty region","Active sheet Save did not retain accepted draft");
        a.SelectAsset(Project::GetAssetDirectory()/std::filesystem::u8path(u8"Prefabs/é independent.hprefab"));
        auto prefab=Prefab::GetEntity(a.m_PrefabScene);prefab.GetComponent<TagComponent>().Tag="Saved active prefab";
        const auto savedScene=Read(e.m_EditorScenePath);
        a.m_ActiveDocument=EditorDocument::Prefab;
        Check(a.SaveActive()&&Read(e.m_EditorScenePath)==savedScene&&Prefab::GetEntity(Prefab::Load(Project::GetAssetDirectory(),std::filesystem::u8path(a.m_PrefabReference))).GetName()=="Saved active prefab","Active prefab Save wrote scene or omitted prefab");
        a.ClosePrefab();
        auto entity=scene->GetEntityByUUID(901);entity.GetComponent<TagComponent>().Tag="Saved active scene";a.m_ActiveDocument=EditorDocument::Scene;
        Check(a.SaveActive()&&a.SceneText()==a.m_SavedScene,"Active scene Save failed");
        const auto saved=a.m_SavedScene;
        auto& box=entity.GetComponent<BoxCollider2DComponent>();const auto size=box.Size;box.Size.x=0;
        Check(!e.OnSceneSimulate(true)&&e.m_ActiveScene==scene&&e.m_SceneState==EditorLayer::SceneState::Edit&&a.m_SavedScene==saved,"Invalid Simulate started physics or lost editor draft");box.Size=size;
        RecoveryRetryChecks();
        std::cout<<"PASS: production Save-and-Assign failure/success, stale target rejection, failed Open draft preservation, busy callback/save guards, active sheet/prefab/scene Save and Simulate preflight\n";
    }
    void RecoveryRetryChecks() {
        auto& e=m_Editor;auto& a=*e.m_Authoring;auto& panel=a.m_Sprites;
        const auto scene=e.m_EditorScene;const auto sceneText=SceneSerializer(scene).SerializeText();
        const auto entitySelection=e.m_SceneHierarchyPanel.GetSelectedEntity().GetUUID();
        const auto pathA=Project::GetAssetDirectory()/panel.m_Document->Reference();
        const auto pathB=Project::GetAssetDirectory()/"Textures/retry-recovery.hsprites";
        const auto savedA=Read(pathA);
        panel.m_Document->Draft().Regions.front().Name="Unsaved recovery draft A";
        panel.m_Document->Changed();
        panel.m_Selected=panel.m_Document->Draft().Regions.front().ID;
        panel.m_SelectedClip=panel.m_Document->Draft().Clips.front().ID;
        const auto documentA=panel.m_Document.get();const auto identityA=panel.Identity();
        const auto selected=panel.m_Selected,selectedClip=panel.m_SelectedClip;
        const auto draftA=WriteSpriteSheetText(panel.m_Document->Draft());
        auto write=[&](const auto& path,const auto& text){FileSystem::WriteFileAtomically(path,[&](auto& out){out<<text;});};
        auto malformed=[&]{write(pathB,"SpriteSheetVersion: 99\n");};
        auto retained=[&]{
            Check(panel.m_Document.get()==documentA&&panel.Identity()==identityA&&panel.Dirty()&&
                  WriteSpriteSheetText(panel.m_Document->Draft())==draftA&&panel.m_Selected==selected&&panel.m_SelectedClip==selectedClip,
                  "Recovery retry lost sheet A, draft or selection");
            Check(e.m_EditorScene==scene&&SceneSerializer(scene).SerializeText()==sceneText&&
                  e.m_SceneHierarchyPanel.GetSelectedEntity().GetUUID()==entitySelection,
                  "Recovery retry changed scene/session selection");
            Check(panel.m_Recovery&&panel.m_RecoveryPath==pathB&&!panel.m_RecoveryError.empty(),
                  "Recovery retry lost usable recovery information");
        };
        auto resolve=[&](GuardChoice choice){
            a.m_ResolvingDocumentAction=true;
            const bool result=a.m_Documents.Resolve(choice,a.Documents(),[&](const auto& doc){return a.SaveDocument(doc);});
            a.m_ResolvingDocumentAction=false;return result;
        };
        auto guarded=[&]{
            Check(a.m_Documents.Pending()&&a.m_Documents.Intent()==OperationIntent::OpenSheet&&
                  a.m_Documents.Affected().size()==1&&a.m_Documents.Affected().front().Identity==identityA,
                  "Production recovery retry skipped the sheet document guard");
        };
        malformed();a.SelectAsset(pathB);guarded();
        Check(!resolve(GuardChoice::Discard),"Malformed recovery fixture unexpectedly opened");retained();
        resolve(GuardChoice::Cancel);write(pathB,savedA);
        panel.RequestRetryOpen();guarded();
        Check(!panel.RetryOpenAvailability()&&std::string(panel.RetryOpenAvailability().Reason)=="Resolve the pending document operation first",
              "Recovery retry did not explain pending-operation availability");
        resolve(GuardChoice::Cancel);retained();

        panel.RequestRetryOpen();guarded();
        const auto recoveryError=panel.m_RecoveryError;
        write(pathA,savedA+"\n# External edit: deterministic save conflict\n");
        const auto externalA=Read(pathA);
        Check(!resolve(GuardChoice::SaveAndContinue)&&a.m_Documents.Pending()&&
              a.m_Documents.Results().size()==1&&a.m_Documents.Results().front().Outcome==SaveOutcome::Failed,
              "Failed sheet save allowed recovery replacement");
        retained();Check(panel.m_RecoveryError==recoveryError&&!panel.m_Error.empty()&&Read(pathA)==externalA,
                         "Failed save lost open diagnostic or changed conflicting file");
        write(pathA,savedA);resolve(GuardChoice::Cancel);

        bool unrelatedExecuted=false;
        a.Guard(OperationIntent::SaveAll,[&]{unrelatedExecuted=true;return true;});
        panel.RequestRetryOpen();retained();
        Check(a.m_Documents.Pending()&&a.m_Documents.Intent()==OperationIntent::SaveAll&&!unrelatedExecuted,
              "Recovery retry bypassed or replaced an existing operation");
        resolve(GuardChoice::Cancel);
        const auto availability=panel.Availability;
        panel.Availability=[](EditorAction){return ActionAvailability{"Open a project first"};};
        panel.RequestRetryOpen();
        Check(!a.m_Documents.Pending()&&!panel.RetryOpenAvailability(),"Recovery retry ignored current action availability");
        retained();panel.Availability=availability;

        panel.RequestRetryOpen();guarded();malformed();
        Check(!resolve(GuardChoice::Discard),"Failed retry discarded before successful replacement");retained();
        resolve(GuardChoice::Cancel);write(pathB,savedA);
        panel.RequestRetryOpen();guarded();
        // A deferred retry must use the originally requested B, even if recovery state changes.
        panel.m_RecoveryPath=Project::GetAssetDirectory()/"Textures/subsequently-changed.hsprites";
        Check(resolve(GuardChoice::Discard)&&!a.m_Documents.Pending()&&panel.Identity()!=identityA&&
              panel.m_Document->Reference()=="Textures/retry-recovery.hsprites"&&!panel.Dirty()&&
              !panel.m_Recovery&&panel.m_RecoveryPath.empty()&&panel.m_RecoveryError.empty()&&panel.m_Error.empty(),
              "Successful guarded retry used a changed path or failed to clear recovery/update identity");
        Check(Read(pathA)==savedA&&SceneSerializer(scene).SerializeText()==sceneText,
              "Discard-on-success wrote sheet A or changed the scene");
        // Restore the normal smoke's clean sheet for its existing render/assignment assertions.
        a.SelectAsset(pathA);
        Check(panel.m_Document->Reference()=="Textures/inspector.hsprites"&&!panel.Dirty(),"Recovery regression did not restore clean smoke sheet");
        std::cout<<"PASS: production recovery retry guard, cancel, save failure, failed discard, pending/availability rejection, captured path and successful replacement\n";
    }
    void AuthoringChecks() {
        auto root=Project::GetAssetDirectory(); auto scene=CreateRef<Scene>();auto source=scene->CreateEntity("Prefab authored");
        source.GetComponent<TransformComponent>().Translation={6,4,.2f};
        source.GetComponent<TransformComponent>().Scale={.4f,.5f,1.0f};
        source.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
        source.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
        source.AddComponent<BoxCollider2DComponent>();
        auto& fields=ScriptEngine::GetScriptFieldMap(source);
        fields["Speed"].Field={ScriptFieldType::Float,"Speed",nullptr};fields["Speed"].SetValue<float>(6);
        fields["Target"].Field={ScriptFieldType::Entity,"Target",nullptr};fields["Target"].SetValue<uint64_t>(source.GetUUID());
        Prefab::Save(root,std::filesystem::u8path(u8"Prefabs/é independent.hprefab"),scene,source);
        auto target=CreateRef<Scene>();TransformComponent initial;initial.Translation={2,3,0};initial.Scale={2,2,1};
        auto first=Prefab::Instantiate(root,std::filesystem::u8path(u8"Prefabs/é independent.hprefab"),*target,initial);
        auto second=Prefab::Instantiate(root,std::filesystem::u8path(u8"Prefabs/é independent.hprefab"),*target,initial);
        Check(first.GetUUID()!=second.GetUUID() && first.GetUUID()!=source.GetUUID(),"Prefab reused identity");
        auto& a=ScriptEngine::GetScriptFieldMap(first);auto& b=ScriptEngine::GetScriptFieldMap(second);
        Check(a.at("Target").GetValue<uint64_t>()==first.GetUUID() && b.at("Target").GetValue<uint64_t>()==second.GetUUID(),"Prefab self reference not remapped");
        a.at("Speed").SetValue<float>(9);Check(b.at("Speed").GetValue<float>()==6,"Prefab fields shared ownership");
        Check(!first.GetComponent<Rigidbody2DComponent>().RuntimeBody && !first.GetComponent<BoxCollider2DComponent>().RuntimeFixture,"Prefab borrowed physics");
        auto preservedScale=Prefab::Instantiate(root,std::filesystem::u8path(u8"Prefabs/é independent.hprefab"),*target,initial,false);
        Check(preservedScale.GetComponent<TransformComponent>().Scale==source.GetComponent<TransformComponent>().Scale,"Position-only prefab placement lost authored scale");
        m_Editor.m_Authoring->m_InitialTransform=initial;
        m_Editor.m_Authoring->InstantiatePrefab(root/std::filesystem::u8path(u8"Prefabs/é independent.hprefab"));
        auto dropped=m_Editor.m_SceneHierarchyPanel.GetSelectedEntity();
        Check(dropped && dropped.GetComponent<TransformComponent>().Translation==source.GetComponent<TransformComponent>().Translation && dropped.GetComponent<TransformComponent>().Scale==source.GetComponent<TransformComponent>().Scale,"Viewport prefab drop reused another asset's inspector placement");
        const auto before=target->GetAllEntitiesWith<IDComponent>().size();
        FileSystem::WriteFileAtomically(root/"Prefabs/broken.hprefab",[](auto& out){out<<"PrefabVersion: 99\nEntities: []\n";});
        bool rejected=false;try{Prefab::Instantiate(root,"Prefabs/broken.hprefab",*target,initial);}catch(const std::exception&){rejected=true;}
        Check(rejected && target->GetAllEntitiesWith<IDComponent>().size()==before,"Malformed prefab mutated target scene");
        fields["Target"].SetValue<uint64_t>(123);rejected=false;
        try{Prefab::Save(root,"Prefabs/external.hprefab",scene,source);}catch(const std::exception&){rejected=true;}
        Check(rejected && !std::filesystem::exists(root/"Prefabs/external.hprefab"),"Prefab silently bound external entity");
        auto childScene=CreateRef<Scene>();auto child=childScene->CreateEntity("Dynamic child");child.AddComponent<ScriptComponent>().ClassName="Migration.LifecycleChild";auto& childBody=child.AddComponent<Rigidbody2DComponent>();childBody.GravityScale=0;childBody.Type=Rigidbody2DComponent::BodyType::Dynamic;child.AddComponent<BoxCollider2DComponent>();
        Prefab::Save(root,"Prefabs/child.hprefab",childScene,child);
        auto runtimeScene=CreateRef<Scene>();auto spawner=runtimeScene->CreateEntity("Spawner");spawner.AddComponent<ScriptComponent>().ClassName="Migration.LifecycleSpawner";
        auto& childField=ScriptEngine::GetScriptFieldMap(spawner)["Child"];childField.Field={ScriptFieldType::Prefab,"Child",nullptr};childField.AssetReference="Prefabs/child.hprefab";
        childField.AssetReference="Prefabs/missing.hprefab";rejected=false;
        try{Prefab::Save(root,"Prefabs/missing-reference.hprefab",runtimeScene,spawner);}catch(const std::exception&){rejected=true;}
        Check(rejected&&!std::filesystem::exists(root/"Prefabs/missing-reference.hprefab"),"Missing typed prefab reference was accepted");
        childField.AssetReference="Prefabs/child.hprefab";
        RuntimeSession runtime;runtime.Start(Project::GetActive(),runtimeScene);runtime.Update(.01f);
        auto current=runtime.GetScene();auto dynamic=current->FindEntityByName("Dynamic child");Check(bool(dynamic),"Managed instantiation failed");
        auto dynamicID=dynamic.GetUUID();auto instance=ScriptEngine::GetEntityScriptInstance(dynamicID);Check(instance&&instance->GetFieldValue<int>("Creates")==1&&instance->GetFieldValue<int>("Updates")==0&&instance->GetFieldValue<float>("InitialX")==7&&instance->GetFieldValue<float>("InitialVelocityX")==1&&instance->GetFieldValue<bool>("BodyReady"),"Dynamic startup/transform/immediate physics/first update contract failed");
        runtime.Update(.01f);auto parent=ScriptEngine::GetEntityScriptInstance(spawner.GetUUID());
        Check(parent->GetFieldValue<bool>("InvalidatedImmediately")&&!current->GetEntityByUUID(dynamicID)&&!instance->GetManagedObject(),"Repeated managed destruction did not invalidate/cleanup");
        runtime.Update(.01f);Check(!current->GetEntityByUUID(spawner.GetUUID())&&!parent->GetManagedObject(),"Self destruction retained managed handle");runtime.Stop();
        runtime.Start(Project::GetActive(),runtimeScene);auto pending=Prefab::Instantiate(root,"Prefabs/child.hprefab",*runtime.GetScene(),initial);auto pendingID=pending.GetUUID();auto retained=runtime.GetScene();runtime.Stop();
        Check(!retained->GetEntityByUUID(pendingID)&&!retained->IsRunning(),"Stop did not cancel pending instantiation");
        EditorPreferences settings;settings.SDK=m_Directory.generic_u8string();settings.Python=(m_Directory/"Python é/python").generic_u8string();settings.UIScale=1.25f;settings.Remember(m_Editor.m_ProjectPath);settings.Save();
        std::string diagnostic;auto loaded=EditorPreferences::Load(diagnostic);
        Check(diagnostic.empty() && loaded.SDK==settings.SDK && loaded.Python==settings.Python && loaded.UIScale==1.25f && loaded.RecentProjects==settings.RecentProjects,"Preferences persistence failed");
        auto location=EditorPreferences::Location();FileSystem::WriteFileAtomically(location,[](auto& out){out<<"Version: 99\n";});
        loaded=EditorPreferences::Load(diagnostic);Check(!diagnostic.empty() && loaded.SDK.empty() && loaded.UIScale==1 && Read(location)=="Version: 99\n","Malformed preferences not recovered/preserved");
        { AuthoringPanel recovery(m_Editor); recovery.RememberProject();Check(Read(location)=="Version: 99\n" && !recovery.m_PreferenceRecovery.empty(),"Automatic recent-project persistence overwrote recovered preferences"); }
        settings.Save();
        Check(ScriptSource::ValidIdentifier("Player_2")&&!ScriptSource::ValidIdentifier("class")&&!ScriptSource::ValidIdentifier("a/b")&&ScriptSource::ValidNamespace("Game.Play")&&!ScriptSource::ValidNamespace("Game..Play"),"Script identifier validation failed");
        auto created=ScriptSource::Create(root,"AuthoringProbe","Game.Play");Check(Read(created).find("Entity.Instantiate")!=std::string::npos,"Missing generated lifecycle sample");
        Check(ScriptSource::Find(root,"Game.Play.AuthoringProbe")==created,"Script source resolution ignored its namespace");
        rejected=false;try{ScriptSource::Create(root,"AuthoringProbe","Game.Play");}catch(const std::exception&){rejected=true;}Check(rejected,"Script creation overwrote source");
        auto invalid=Toolchain::DiscoverPython(m_Directory/"missing-python");Check(!invalid && invalid.Source.find("Configured")!=std::string::npos,"Invalid configured Python silently fell back");
        std::filesystem::create_directories(m_Directory/"scripts");
        FileSystem::WriteFileAtomically(m_Directory/"scripts/hazel.py",[](auto& out){out<<"# cwd is not a configured SDK\n";});
        auto missingTools=ProjectTools::Execute({m_Directory/"missing-python",{},{},{"authoring-preflight"},"Missing tools"});
        Check(!missingTools.Success && missingTools.Output.find("SDK also unavailable")!=std::string::npos,"Missing Python concealed missing SDK or inferred SDK from cwd");
        const auto executable=FileSystem::GetExecutablePath();
        for(const auto& name:{"unsupported-python","malformed-python","failed-python","hang-python"}) {
            auto probe=m_Directory/(std::string(name)+executable.extension().u8string());
            std::error_code linkError;
            std::filesystem::create_hard_link(executable,probe,linkError);
            if(linkError) std::filesystem::copy_file(executable,probe);
            auto selection=Toolchain::ProbePython(probe,"Controlled probe fixture");
            Check(!selection&&!selection.Error.empty(),"Broken/unsupported/timed out Python probe accepted");
            std::filesystem::remove(probe);
        }
        Check(!Toolchain::SelectPythonCandidates({}),"Missing Python was accepted");
        auto probeFolder=m_Directory/std::filesystem::u8path("Python space é");std::filesystem::create_directory(probeFolder);
        auto validProbe=probeFolder/(std::string("valid-python")+executable.extension().u8string());
        auto olderProbe=probeFolder/(std::string("older-python")+executable.extension().u8string());
        std::filesystem::copy_file(executable,validProbe);std::filesystem::copy_file(executable,olderProbe);
        auto ordered=Toolchain::SelectPythonCandidates({olderProbe,validProbe});
        Check(bool(ordered)&&ordered.Version=="3.9.1"&&ordered.Executable==olderProbe,"Python installation ordering was nondeterministic");
        ordered=Toolchain::SelectPythonCandidates({m_Directory/"missing",validProbe,olderProbe});
        Check(bool(ordered)&&ordered.Version=="3.14.1"&&ordered.Executable==validProbe,"Compatible Python candidate selection failed");
        std::filesystem::remove(validProbe);std::filesystem::remove(olderProbe);
        auto python=Toolchain::DiscoverPython();Check(bool(python)&&python.Executable.is_absolute()&&!python.Version.empty(),"Installed Python discovery failed");
        std::cout<<"AUTHORING PYTHON: "<<python.Executable.generic_u8string()<<"; "<<python.Source<<"; "<<python.Version<<"\n";
        Check(bool(Toolchain::DiscoverPython(python.Executable)),"Explicit valid Python failed");
#ifndef HZ_PLATFORM_WINDOWS
        auto savedPath=std::getenv("PATH")?std::getenv("PATH"):std::string{};setenv("PATH","/nonexistent-hazel-test",1);
        auto without=Toolchain::DiscoverPython();setenv("PATH",savedPath.c_str(),1);
        Check(bool(without)&&without.Executable==python.Executable,"Python discovery depends on PATH or is nondeterministic");
        auto alias=m_Directory/std::filesystem::u8path("Python space é");std::filesystem::create_directories(alias);std::filesystem::create_symlink(python.Executable,alias/"interpreter");
        Check(bool(Toolchain::DiscoverPython(alias/"interpreter")),"Unicode/spaces interpreter probe failed");
        std::filesystem::remove(alias/"interpreter");Check(!Toolchain::DiscoverPython(alias/"interpreter"),"Removed interpreter cache remained valid");
        auto timed=Process::Run("/bin/sleep",{"2"},{},std::chrono::milliseconds(50));Check(timed.TimedOut,"Process timeout did not reap child");
#ifdef HZ_PLATFORM_LINUX
        auto descriptor=::open((m_Directory/"AuthoringProject/Authoring.hproj").c_str(),O_RDONLY);
        Check(descriptor>=0,"Cannot open descriptor fixture");
        auto inherited=fcntl(descriptor,F_DUPFD,100);::close(descriptor);
        Check(inherited>=100,"Cannot create inheritable descriptor fixture");
        auto closed=Process::Run(executable,{"--probe-closed-descriptor",std::to_string(inherited)},{},std::chrono::seconds(5));
        auto resultPath=m_Directory/"launch-descriptors.txt";
        bool launched=Process::Launch(executable,{"--probe-closed-descriptor",std::to_string(inherited),resultPath.generic_u8string()});
        for(int wait=0;launched && Read(resultPath)!="closed" && wait<200;wait++)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ::close(inherited);
        Check(closed.ExitCode==0 && launched && Read(resultPath)=="closed","Tool/external-editor child inherited engine descriptors");
#endif
#else
        auto savedPath=FileSystem::GetEnvironmentPath("PATH").wstring();_wputenv_s(L"PATH",L"C:/nonexistent-hazel-test");
        auto without=Toolchain::DiscoverPython();_wputenv_s(L"PATH",savedPath.c_str());
        Check(bool(without)&&without.Executable==python.Executable,"Windows Python discovery depends on PATH or is nondeterministic");
        std::filesystem::create_directory(m_Directory/"WindowsApps");std::filesystem::copy_file(executable,m_Directory/"WindowsApps/python.exe");
        auto storeAlias=Toolchain::ProbePython(m_Directory/"WindowsApps/python.exe","alias");Check(!storeAlias&&storeAlias.Error.find("aliases")!=std::string::npos,"Windows execution alias accepted");
#endif
        std::cout<<"PASS: prefab identity/independence/self/external references/malformed assets, script creation, preferences recovery/scopes, absolute Python discovery\n";
    }
    void FailureChecks() {
        auto& e=m_Editor;
        const auto project=Project::GetActive(); const auto scene=e.m_ActiveScene; const auto authored=e.m_EditorScene;
        const auto state=e.m_SceneState; const auto projectPath=e.m_ProjectPath; const auto scenePath=e.m_EditorScenePath;
        const auto speed=ScriptEngine::GetScriptFieldMap(authored->GetEntityByUUID(901)).at("Speed").GetValue<float>();
        const auto instance=ScriptEngine::GetEntityScriptInstance(901);
        auto preserved=[&]() {
            Check(Project::GetActive()==project && e.m_ActiveScene==scene && e.m_EditorScene==authored &&
                  e.m_SceneState==state && e.m_ProjectPath==projectPath && e.m_EditorScenePath==scenePath,
                  "Failed action replaced valid editor session");
            Check(ScriptEngine::GetScriptFieldMap(authored->GetEntityByUUID(901)).at("Speed").GetValue<float>()==speed,
                  "Failed action changed authored fields");
            Check(!e.m_ActionError.empty(),"Failed action did not report actionable error");
            if(instance) Check(instance->GetManagedObject() && ScriptEngine::GetEntityScriptInstance(901)==instance,
                               "Failed action destroyed live managed domain");
        };
        const auto badProject=projectPath.parent_path()/"bad.hproj";
        const auto badScene=projectPath.parent_path()/"broken.hazel";
        std::ofstream(badProject)<<"Project: [unterminated";
        std::ofstream(badScene)<<"Scene: [unterminated";
        for(const auto& path:{badProject,projectPath.parent_path()/"missing.hproj"}) {
            Check(!e.OpenProject(path),"Invalid project reported success"); preserved();
        }
        for(const auto& path:{badScene,projectPath.parent_path()/"missing.hazel",badProject}) {
            Check(!e.OpenScene(path),"Invalid scene reported success"); preserved();
        }
        const auto missingTexture=projectPath.parent_path()/"missing-texture.hazel";
        std::ofstream(missingTexture)<<"Scene: Missing texture\nEntities:\n  - Entity: 901\n    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TexturePath: Textures/missing.png\n      TilingFactor: 1\n";
        Check(!e.OpenScene(missingTexture),"Missing texture scene reported success"); preserved();
        auto candidate=CreateRef<Project>(); candidate->GetConfig()=project->GetConfig();
        for(bool corrupt:{false,true}) {
            candidate->GetConfig().StartScene=corrupt ? std::filesystem::absolute(badScene) : std::filesystem::path("Scenes/missing.hazel");
            Check(ProjectSerializer(candidate).Serialize(badProject),"Failure project setup failed");
            Check(!e.OpenProject(badProject),"Invalid start scene reported success"); preserved();
        }
        candidate->GetConfig()=project->GetConfig(); candidate->GetConfig().AssetDirectory="missing-assets";
        Check(ProjectSerializer(candidate).Serialize(badProject),"Missing asset setup failed");
        Check(!e.OpenProject(badProject),"Missing assets reported success"); preserved();
        candidate->GetConfig()=project->GetConfig(); candidate->GetConfig().ScriptModulePath="Scripts/missing.dll";
        Check(ProjectSerializer(candidate).Serialize(badProject),"Missing assembly setup failed");
        Check(!e.OpenProject(badProject),"Missing assembly reported success"); preserved();
        candidate->GetConfig()=project->GetConfig();
        Check(ProjectSerializer(candidate).Serialize(badProject),"Corrupt assembly setup failed");
        const auto assembly=Project::GetAssetFileSystemPath(project->GetConfig().ScriptModulePath);
        const auto backup=m_Directory/"valid-scripts.dll";
        std::filesystem::copy_file(assembly,backup,std::filesystem::copy_options::overwrite_existing);
        std::ofstream(assembly,std::ios::binary|std::ios::trunc)<<"corrupt assembly";
        Check(!e.OpenProject(badProject),"Corrupt assembly project reported success"); preserved();
        Check(!e.ReloadScripts(),"Corrupt reload reported success"); preserved();
        std::filesystem::copy_file(backup,assembly,std::filesystem::copy_options::overwrite_existing);
        const auto core=m_Directory/"assets/Scripts/Hazel-ScriptCore.dll";
        const auto coreBackup=m_Directory/"valid-core.dll";
        std::filesystem::copy_file(core,coreBackup,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(core);
        Check(!e.OpenProject(badProject),"Missing ScriptCore reported success"); preserved();
        std::ofstream(core,std::ios::binary|std::ios::trunc)<<"corrupt ScriptCore";
        Check(!e.OpenProject(badProject),"Corrupt ScriptCore reported success"); preserved();
        // A valid managed image with the wrong ScriptCore API must also fail before commitment.
        std::filesystem::copy_file(backup,core,std::filesystem::copy_options::overwrite_existing);
        Check(!e.OpenProject(badProject),"Incompatible ScriptCore reported success"); preserved();
        std::filesystem::copy_file(coreBackup,core,std::filesystem::copy_options::overwrite_existing);
        const auto priorScene=Read(scenePath), priorProject=Read(projectPath);
        Check(!e.SerializeScene(authored,m_Directory/"missing-dir/save.hazel"),"Failed scene save reported success"); preserved();
        const auto blocked=m_Directory/"blocked.hazel";
        std::filesystem::create_directories(blocked); std::ofstream(blocked/"sentinel")<<"intact";
        Check(!e.SerializeScene(authored,blocked) && Read(blocked/"sentinel")=="intact","Failed scene replacement damaged destination"); preserved();
        e.m_ProjectPath=m_Directory/"missing-dir/save.hproj";
        Check(!e.SaveProject(),"Failed project save reported success"); e.m_ProjectPath=projectPath; preserved();
        Check(Read(scenePath)==priorScene && Read(projectPath)==priorProject,"Failed save changed prior scene/project bytes");
    }
    EditorLayer& m_Editor;
    std::filesystem::path m_Directory;
    bool& m_Done;
    int m_Frame = 0;
    bool m_Trickle = true;
};
}
int main(int argc, char** argv) {
#ifdef HZ_PLATFORM_WINDOWS
    auto encoded = Hazel::WindowsCommandLineUTF8();
    std::vector<char*> pointers;
    for (auto& argument : encoded) pointers.push_back(argument.data());
    argc = static_cast<int>(pointers.size()); pointers.push_back(nullptr); argv = pointers.data();
#endif
    using namespace Hazel;
#ifdef HZ_PLATFORM_LINUX
    if(argc>=3 && std::string(argv[1])=="--probe-closed-descriptor") {
        bool closed=fcntl(std::stoi(argv[2]),F_GETFD)==-1 && errno==EBADF;
        if(argc==4)std::ofstream(std::filesystem::u8path(argv[3]))<<(closed?"closed":"inherited");
        return closed?0:1;
    }
#endif
    if(argc>1 && std::string(argv[1])=="-I") {
        auto name=std::filesystem::u8path(argv[0]).filename().u8string();
        if(name.find("valid")!=std::string::npos || name.find("older")!=std::string::npos) {
            std::cout<<"{\"version\":[3,"<<(name.find("older")!=std::string::npos?9:14)<<",1],\"ok\":true,\"executable\":"<<std::quoted(FileSystem::GetExecutablePath().generic_u8string())<<"}\n";return 0;
        }
        if(name.find("unsupported")!=std::string::npos){std::cout<<"{\"version\":[3,8,0],\"ok\":true,\"executable\":\"/missing\"}\n";return 0;}
        if(name.find("malformed")!=std::string::npos){std::cout<<"malformed probe output\n";return 0;}
        if(name.find("hang")!=std::string::npos)std::this_thread::sleep_for(std::chrono::seconds(10));
        return 7;
    }
    const auto previous = std::filesystem::current_path();
    const auto directory = std::filesystem::temp_directory_path() / std::filesystem::u8path("hazel-editor-é-" + std::to_string(std::random_device{}()));
    try {
        Log::Init(); Check(argc == 4, "Usage: EditorSmoke Core.dll Regression.dll test-environment-dir");
        auto core = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        auto scripts = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        auto source = std::filesystem::absolute(std::filesystem::u8path(argv[3]));
        std::filesystem::create_directory(directory);
        std::filesystem::create_directories(directory / "assets/Icons");
        for (const char* name : { "assets", "AuthoringProject" })
            std::filesystem::copy(source / name, directory / name, std::filesystem::copy_options::recursive);
        std::filesystem::create_directories(directory / "assets/Scripts");
        std::filesystem::create_directories(directory / "AuthoringProject/Assets/Scripts/Binaries");
        std::filesystem::copy_file(core, directory / "assets/Scripts/Hazel-ScriptCore.dll");
        std::filesystem::copy_file(scripts, directory / "AuthoringProject/Assets/Scripts/Binaries/Regression.dll", std::filesystem::copy_options::overwrite_existing);
        auto project = directory / "AuthoringProject/Authoring.hproj";
        auto projectArgument = project.lexically_relative(directory).generic_u8string();
        char executable[] = "EditorSmoke"; char* arguments[] = { executable, projectArgument.data() };
        bool done = false;
        {
            ApplicationSpecification spec; spec.Name = "Migration Editor";
            spec.Resources.Root = directory / "assets"; std::filesystem::current_path(directory); spec.CommandLineArgs = { 2, arguments };
            Application application(spec);
            glfwHideWindow(static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow()));
            std::cout << "Renderer: " << glGetString(GL_RENDERER) << "; Version: " << glGetString(GL_VERSION) << '\n';
            auto editor = CreateScope<EditorLayer>(); auto* observer = editor.get();
            application.PushLayer(std::move(editor));
            application.PushLayer(CreateScope<EditorWorkflowSmoke>(*observer, directory, done));
            application.Run();
        }
        Check(done && !ScriptEngine::IsInitialized() && !Application::TryGet(), "Editor workflow/shutdown failed");
        std::filesystem::current_path(previous);
#ifdef HZ_PLATFORM_WINDOWS
        // Diagnose the reproduced Windows-only cleanup lock separately from the
        // actual editor/shutdown assertions; keep deletion strict for this probe.
        std::error_code cleanupError; std::filesystem::remove_all(directory, cleanupError);
        if (cleanupError) {
            wchar_t nativeDirectory[32768]{};
            GetCurrentDirectoryW(32768, nativeDirectory);
            std::cerr << "Cleanup diagnostic: error=" << cleanupError.value()
                << "; cwd=" << std::filesystem::current_path().generic_u8string()
                << "; native-cwd=" << std::filesystem::path(nativeDirectory).generic_u8string() << '\n';
            std::error_code scanError;
            for (auto it = std::filesystem::recursive_directory_iterator(directory, scanError);
                 !scanError && it != std::filesystem::recursive_directory_iterator(); it.increment(scanError))
                std::cerr << "Remaining fixture: " << it->path().generic_u8string() << '\n';
            throw std::filesystem::filesystem_error("Editor fixture cleanup", directory, cleanupError);
        }
#else
        std::filesystem::remove_all(directory);
#endif
        std::cout << "PASS: actual docked editor/panels, Unicode content paths, save/reopen/project, duplicate, gizmo shortcuts/drawing, play/pause/step/simulate/stop and shutdown\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored; std::filesystem::current_path(previous, ignored);
        std::filesystem::remove_all(directory, ignored);
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
