#include "EditorLayer.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
namespace Hazel {
bool EditorLayer::NavigationBlocked() const {
    auto& context=*ImGui::GetCurrentContext();auto* viewport=ImGui::FindWindowByName("Viewport");
    // ImGui's fallback scene-image press is a viewport MoveId, not another widget.
    const bool ownImage=viewport&&context.ActiveId==viewport->MoveId&&(m_ViewportHovered||m_Navigation.Active());
    return context.IO.WantTextInput||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)||
        (ImGui::IsAnyItemActive()&&!ownImage)||ImGuizmo::IsUsing()||m_UIEdit.Active();
}
void EditorLayer::UpdateNavigation(Timestep ts) {
    if(m_SceneState==SceneState::Play||m_ViewportSize.x<=0||m_ViewportSize.y<=0){m_Navigation.Cancel();Application::Get().GetWindow().SetPointerCaptured(false);return;}
    auto& io=ImGui::GetIO();NavigationInput s;
    s.Pointer=Application::Get().GetWindow().IsPointerCaptured()?Input::GetMouseScreenPosition():glm::vec2(io.MousePos.x,io.MousePos.y);s.Left=io.MouseDown[0];s.Right=io.MouseDown[1];s.Middle=io.MouseDown[2];
    s.Alt=io.KeyAlt;s.Shift=io.KeyShift;s.Space=Input::IsKeyPressed(Key::Space);
    s.Movement={float(Input::IsKeyPressed(Key::D))-float(Input::IsKeyPressed(Key::A)),float(Input::IsKeyPressed(Key::E))-float(Input::IsKeyPressed(Key::Q)),float(Input::IsKeyPressed(Key::W))-float(Input::IsKeyPressed(Key::S))};
    if(io.KeyCtrl)s.Speed*=.2f;
    s.Focused=Application::Get().GetWindow().IsFocused();
    s.Blocked=NavigationBlocked();s.Escape=Input::IsKeyPressed(Key::Escape);
    s.OverImage=m_ViewportHovered&&s.Pointer.x>=m_ViewportBounds[0].x&&s.Pointer.y>=m_ViewportBounds[0].y&&s.Pointer.x<m_ViewportBounds[1].x&&s.Pointer.y<m_ViewportBounds[1].y;
    const bool wasActive=m_Navigation.Active();
    m_Navigation.Update(m_NavigationCamera,s,ts);
    if(!wasActive&&m_Navigation.Active()){
        auto* viewport=ImGui::FindWindowByName("Viewport");auto& context=*ImGui::GetCurrentContext();
        if(viewport&&context.ActiveId==viewport->MoveId){ImGui::ClearActiveID();context.MovingWindow=nullptr;}
    }
    auto& window=Application::Get().GetWindow();const bool capture=m_Navigation.Current()==ViewportNavigation::Gesture::Fly;
    if(window.IsPointerCaptured()!=capture){window.SetPointerCaptured(capture);m_Navigation.SeedPointer();}
}
}
