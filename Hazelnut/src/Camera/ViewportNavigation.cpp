#include "EditorLayer.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Assets/ProjectAssets.h"
#include <limits>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
namespace Hazel
{
bool EditorLayer::NavigationBlocked() const
{
    auto &context = *ImGui::GetCurrentContext();
    auto *viewport = ImGui::FindWindowByName("Viewport");
    // ImGui's fallback scene-image press is a viewport MoveId, not another widget.
    const bool ownImage = viewport && context.ActiveId == viewport->MoveId &&
                          (m_ViewportHovered || m_Navigation.Active());
    return context.IO.WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) ||
           (ImGui::IsAnyItemActive() && !ownImage) || ImGuizmo::IsUsing() || m_UIEdit.Active();
}
bool EditorLayer::RouteNavigationScroll(float steps,bool focused)
{
    if(m_SceneState==SceneState::Play||!focused||!m_ViewportHovered||NavigationBlocked())return false;
    m_NavigationCamera.Scroll(steps);return true;
}
void EditorLayer::UpdateNavigation(Timestep ts)
{
    if (m_SceneState == SceneState::Play)
        return;
    if (m_ViewportSize.x <= 0 || m_ViewportSize.y <= 0)
    {
        m_Navigation.Cancel();
        Application::Get().GetWindow().SetPointerCaptured(false);
        return;
    }
    auto &io = ImGui::GetIO();
    NavigationInput s;
    s.Pointer = Application::Get().GetWindow().IsPointerCaptured()
                    ? Input::GetMouseScreenPosition()
                    : glm::vec2(io.MousePos.x, io.MousePos.y);
    s.Left = io.MouseDown[0];
    s.Right = io.MouseDown[1];
    s.Middle = io.MouseDown[2];
    s.Alt = io.KeyAlt;
    s.Shift = io.KeyShift;
    s.Space = Input::IsKeyPressed(Key::Space);
    s.Movement = {float(Input::IsKeyPressed(Key::D)) - float(Input::IsKeyPressed(Key::A)),
                  float(Input::IsKeyPressed(Key::E)) - float(Input::IsKeyPressed(Key::Q)),
                  float(Input::IsKeyPressed(Key::W)) - float(Input::IsKeyPressed(Key::S))};
    if (io.KeyCtrl)
        s.Speed *= .2f;
    s.Focused = Application::Get().GetWindow().IsFocused();
    s.Blocked = NavigationBlocked();
    s.Escape = Input::IsKeyPressed(Key::Escape);
    s.OverImage = m_ViewportHovered && s.Pointer.x >= m_ViewportBounds[0].x &&
                  s.Pointer.y >= m_ViewportBounds[0].y && s.Pointer.x < m_ViewportBounds[1].x &&
                  s.Pointer.y < m_ViewportBounds[1].y;
    const bool wasActive = m_Navigation.Active();
    m_Navigation.Update(m_NavigationCamera, s, ts);
    if (!wasActive && m_Navigation.Active())
    {
        auto *viewport = ImGui::FindWindowByName("Viewport");
        auto &context = *ImGui::GetCurrentContext();
        if (viewport && context.ActiveId == viewport->MoveId)
        {
            ImGui::ClearActiveID();
            context.MovingWindow = nullptr;
        }
    }
    auto &window = Application::Get().GetWindow();
    const bool capture = m_Navigation.Current() == ViewportNavigation::Gesture::Fly;
    if (window.IsPointerCaptured() != capture)
    {
        window.SetPointerCaptured(capture);
        m_Navigation.SeedPointer();
    }
}
void EditorLayer::FrameSelection()
{
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    if (!selected || !selected.BelongsTo(m_ActiveScene.get()))
        return;
    glm::vec3 minimum(std::numeric_limits<float>::max()),
        maximum(std::numeric_limits<float>::lowest());
    for (auto id : m_ActiveScene->GetSubtree(selected))
    {
        auto entity = m_ActiveScene->GetEntityByUUID(id);
        auto world = m_ActiveScene->GetWorldTransform(entity);
        glm::vec3 lo(-.5f), hi(.5f);
        if (entity.HasComponent<MeshRendererComponent>())
            if (auto assets = m_ActiveScene->GetAssets())
            {
                auto mesh = assets->Mesh(entity.GetComponent<MeshRendererComponent>().Mesh);
                if (mesh.Accepted)
                {
                    lo = mesh.Accepted->Geometry.Bounds.Min;
                    hi = mesh.Accepted->Geometry.Bounds.Max;
                }
            }
        for (int i = 0; i < 8; ++i)
        {
            auto point = glm::vec3(world * glm::vec4((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y,
                                                     (i & 4) ? hi.z : lo.z, 1));
            minimum = glm::min(minimum, point);
            maximum = glm::max(maximum, point);
        }
    }
    m_NavigationCamera.FrameBounds(minimum, maximum);
}

} // namespace Hazel
