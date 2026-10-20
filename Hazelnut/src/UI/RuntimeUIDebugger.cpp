#include "EditorLayer.h"
#include "Authoring/AuthoringPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <cstdio>
#include <ImGuizmo.h>
namespace Hazel
{
namespace
{
const char *Policy(GameUI::HitPolicy value)
{
    return value == GameUI::HitPolicy::Control ? "Control"
           : value == GameUI::HitPolicy::Block ? "Block"
                                               : "Pass through";
}
unsigned Edges(glm::vec2 mouse, GameUI::Rect rect)
{
    constexpr float reach = 8;
    if (!GameUI::Rect{rect.Position - glm::vec2(reach), rect.Size + glm::vec2(reach * 2)}.Contains(
            mouse))
        return 0;
    unsigned edges = 0;
    if (std::abs(mouse.x - rect.Position.x) <= reach)
        edges |= 1;
    else if (std::abs(mouse.x - rect.Position.x - rect.Size.x) <= reach)
        edges |= 4;
    if (std::abs(mouse.y - rect.Position.y) <= reach)
        edges |= 2;
    else if (std::abs(mouse.y - rect.Position.y - rect.Size.y) <= reach)
        edges |= 8;
    return edges;
}
} // namespace
void EditorLayer::UI_RectEditing()
{
    auto &io = ImGui::GetIO();
    const ImGuiID gestureID = ImGui::GetID("UI resolved rect gesture");
    ImGui::KeepAliveID(gestureID);
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    bool canEdit =
        m_SceneState == SceneState::Edit && m_Authoring->Availability(EditorAction::EditScene);
    if (m_UIEdit.Active() && (!canEdit || !selected || selected.GetUUID() != m_UIEdit.EntityID() ||
                              io.AppFocusLost ||
                              ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
        m_UIEdit.Finish(true);
    const auto &snapshot = m_ActiveScene->GetUISnapshot();
    const auto *n = selected ? snapshot.Find(selected.GetUUID()) : nullptr;
    glm::vec2 displayScale = glm::vec2(1);
    if (snapshot.Surface.Viewport.x > 0 && snapshot.Surface.Viewport.y > 0)
        displayScale = m_ViewportSize / snapshot.Surface.Viewport;
    glm::vec2 mouse{io.MousePos.x, io.MousePos.y};
    unsigned edges = 0;
    bool selectedHandle = false;
    if (n && n->LayoutResolved)
    {
        auto selectedBounds = n->PixelBounds;
        if (n->IsCanvas)
            selectedBounds = {n->ParentContent.Position * n->Scale,
                              n->ParentContent.Size * n->Scale};
        GameUI::Rect display{m_ViewportBounds[0] + selectedBounds.Position * displayScale,
                             selectedBounds.Size * displayScale};
        edges = Edges(mouse, display);
        unsigned allowed=n->ContainerOwned?0:15;
        {allowed=15;if(n->WidthMode!=GameUI::SizeMode::Fixed)allowed&=~5u;if(n->HeightMode!=GameUI::SizeMode::Fixed)allowed&=~10u;if(n->ContainerOwned)allowed&=~3u;}
        selectedHandle = canEdit && !n->IsCanvas && edges && !(edges&~allowed) &&
                         ImGui::IsWindowHovered() && !io.WantTextInput;
        auto draw = ImGui::GetWindowDrawList();
        auto end = display.Position + display.Size;
        draw->AddRect({display.Position.x, display.Position.y}, {end.x, end.y},
                      IM_COL32(255, 190, 70, 255));
        if (canEdit && !n->IsCanvas && allowed)
        {
            const glm::vec2 handles[] = {display.Position,
                                         {end.x, display.Position.y},
                                         end,
                                         {display.Position.x, end.y},
                                         {(display.Position.x + end.x) * .5f, display.Position.y},
                                         {end.x, (display.Position.y + end.y) * .5f},
                                         {(display.Position.x + end.x) * .5f, end.y},
                                         {display.Position.x, (display.Position.y + end.y) * .5f}};
            const unsigned handleEdges[]={3,6,12,9,2,4,8,1};
            for (size_t i=0;i<8;++i)if(!(handleEdges[i]&~allowed))
            {auto point=handles[i];
                draw->AddRectFilled({point.x - 3, point.y - 3}, {point.x + 3, point.y + 3},
                                    IM_COL32(255, 190, 70, 255));
            }
        }
    }
    // Handles and an active gesture retain selection; normal clicks still use
    // renderer picking. Invisible layout groups can be dragged after tree selection.
    if (m_SceneState != SceneState::Play && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
        !ImGuizmo::IsOver() && !io.KeyAlt && !m_UIEdit.Active() && !selectedHandle)
    {
        bool emptyRect = n && n->LayoutResolved && !n->IsCanvas &&
                         !selected.HasComponent<SpriteRendererComponent>() &&
                         !selected.HasComponent<TextComponent>() &&
                         GameUI::Rect{m_ViewportBounds[0] + n->PixelBounds.Position * displayScale,
                                      n->PixelBounds.Size * displayScale}
                             .Contains(mouse);
        if (!emptyRect)
            m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
        selected = m_SceneHierarchyPanel.GetSelectedEntity();
        n = selected ? snapshot.Find(selected.GetUUID()) : nullptr;
    }
    if (canEdit && ImGui::IsMouseClicked(0) && !io.KeyAlt && n && n->LayoutResolved &&
        !n->IsCanvas && (selectedHandle || m_ViewportHovered))
    {
        GameUI::Rect display{m_ViewportBounds[0] + n->PixelBounds.Position * displayScale,
                             n->PixelBounds.Size * displayScale};
        edges = Edges(mouse, display);
        if (edges || display.Contains(mouse))
        {
            const auto target = n->ID;
            m_SceneHierarchyPanel.FinishUIEdit();
            if (m_UIEdit.Begin(m_ActiveScene, target, edges, m_Authoring?&m_Authoring->SceneHistory():nullptr))
            {
                m_UIGestureStart = mouse;
                ImGui::SetActiveID(gestureID, ImGui::GetCurrentWindow());
            }
        }
    }
    if (m_UIEdit.Active() && ImGui::IsMouseDragging(0) && !io.KeyAlt)
        m_UIEdit.Update((mouse - m_UIGestureStart) / displayScale / m_UIEdit.Scale());
    if (m_UIEdit.Active() && ImGui::IsMouseReleased(0))
    {
        // Motion and release can arrive together after the last rendered frame.
        m_UIEdit.Update((mouse - m_UIGestureStart) / displayScale / m_UIEdit.Scale());
        m_UIEdit.Finish(false);
    }
    if (!m_UIEdit.Active() && ImGui::GetActiveID() == gestureID)
        ImGui::ClearActiveID();
    if (!m_UIEdit.Error().empty())
    {
        ImGui::TextWrapped("UI edit: %s", m_UIEdit.Error().c_str());
    }
}
void EditorLayer::UI_DebugOverlay()
{
    if (!m_UIBoundsOverlay && !m_UIClipOverlay && !m_UIHitOverlay)
        return;
    const auto &s = m_ActiveScene->GetUISnapshot();
    if (!s.Valid || s.Surface.Viewport.x <= 0 || s.Surface.Viewport.y <= 0)
        return;
    auto scale = m_ViewportSize / s.Surface.Viewport;
    auto draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({m_ViewportBounds[0].x, m_ViewportBounds[0].y},
                       {m_ViewportBounds[1].x, m_ViewportBounds[1].y}, true);
    for (auto index : s.PainterOrder)
    {
        const auto &n = s.Elements[index];
        auto bounds = [&](GameUI::Rect r, ImU32 color) {
            auto p = m_ViewportBounds[0] + r.Position * scale, q = p + r.Size * scale;
            draw->AddRect({p.x, p.y}, {q.x, q.y}, color);
        };
        if (m_UIBoundsOverlay)
            bounds(n.PixelBounds, IM_COL32(255, 190, 70, 150));
        if(m_UIBoundsOverlay){GameUI::UIImageVisual image;if(m_ActiveScene->GetUIImageVisual(n.ID,image))bounds({image.Placement.Visual.Position*n.Scale,image.Placement.Visual.Size*n.Scale},IM_COL32(100,255,140,220));}
        if (m_UIClipOverlay)
            bounds(n.PixelClip, IM_COL32(90, 210, 255, 220));
        if (m_UIHitOverlay && n.Hit != GameUI::HitPolicy::PassThrough)
        {
            auto p = m_ViewportBounds[0] + n.PixelClip.Position * scale;
            char label[64];
            std::snprintf(label, sizeof(label), "%zu %s", n.Order, Policy(n.Hit));
            draw->AddText({p.x, p.y}, IM_COL32(240, 240, 240, 255), label);
        }
    }
    if(m_UIBoundsOverlay)for(const auto &guide:s.Guides)if(auto owner=s.Find(guide.Owner)){
        auto p=m_ViewportBounds[0]+guide.Bounds.Position*owner->Scale*scale;
        auto q=p+guide.Bounds.Size*owner->Scale*scale;
        draw->AddRect({p.x,p.y},{q.x,q.y},IM_COL32(170,110,255,180));
    }
    draw->PopClipRect();
}
void EditorLayer::UI_Debugger()
{
    if (!m_ShowUIDebugger)
        return;
    ImGui::SetNextWindowSize({440, 500}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Runtime UI Debugger", &m_ShowUIDebugger))
    {
        ImGui::End();
        return;
    }
    const auto &s = m_ActiveScene->GetUISnapshot();
    auto d = m_ActiveScene->GetUIDiagnostics();
    ImGui::Text("Generation %llu | input used %llu | %s", (unsigned long long)s.Generation,
                (unsigned long long)d.InputGeneration, s.Valid ? "valid" : "unresolved/error");
    ImGui::Text("Viewport %.1f x %.1f | framebuffer %.1f x %.1f", s.Surface.Viewport.x,
                s.Surface.Viewport.y, s.Surface.Framebuffer.x, s.Surface.Framebuffer.y);
    ImGui::Text("Focus %llu | capture %llu | modal %llu", (unsigned long long)d.Focus,
                (unsigned long long)d.Capture, (unsigned long long)d.Modal);
    ImGui::Text("Input %s | gameplay blocked %s | pointer blocked %s",
                d.InputEnabled ? "enabled" : "inactive", d.GameplayBlocked ? "yes" : "no",
                d.PointerBlocked ? "yes" : "no");
    ImGui::Text("Resolve %llu | reuse %llu | source checks %llu", (unsigned long long)d.Resolves,
                (unsigned long long)d.Reuses, (unsigned long long)d.SourceChecks);
    ImGui::Text("Last rebuild %.3f ms | routing %.3f ms (CPU)", d.LastResolveMilliseconds,
                d.LastRoutingMilliseconds);
    ImGui::Text("Text measure misses/hits %llu / %llu | arranged nodes %llu",
                (unsigned long long)d.Measurements,(unsigned long long)d.MeasurementReuses,(unsigned long long)d.Arrangements);
    ImGui::Text("Image metric misses/hits %llu / %llu",(unsigned long long)d.ImageMeasurements,(unsigned long long)d.ImageMeasurementReuses);
    ImGui::Text("Rects/hits %zu / %zu | dropped activations %llu", s.PainterOrder.size(),
                s.Buttons.size(), (unsigned long long)d.DroppedActions);
    ImGui::Text("UI rects visible/submitted %u / %u | draw runs %u",d.VisibleElements,d.SubmittedElements,d.DrawCalls);
    if(ImGui::CollapsingHeader("Rendering costs")){
        ImGui::Text("Upload groups %u | clip changes %u",d.UploadGroups,d.ClipTransitions);
        ImGui::Text("CPU source/layout %.3f / %.3f ms",d.LastSourceMilliseconds,d.LastLayoutMilliseconds);
        ImGui::Text("CPU preparation/submission %.3f / %.3f ms",d.LastPrepareMilliseconds,d.LastSubmitMilliseconds);
        if(d.GPUAvailable)ImGui::Text("GPU interval %.3f ms (%llu submissions old)",d.LastGPUMilliseconds,(unsigned long long)(d.PaintSubmission-d.GPUSubmission));
        else ImGui::TextDisabled("GPU interval pending/unavailable");
        ImGui::Text("Text prep rebuild/reuse %llu / %llu | %.1f KiB",(unsigned long long)d.PreparedRebuilds,(unsigned long long)d.PreparedReuses,double(d.PreparedBytes)/1024);
        ImGui::Text("Run changes: primitive %u | font %u",d.PrimitiveTransitions,d.FontTransitions);
        ImGui::Text("Flush: end %u | slots %u | capacity %u | external %u",d.FlushReasons[0],d.FlushReasons[3],d.FlushReasons[4],d.FlushReasons[5]);
    }
    ImGui::Checkbox("Bounds overlay", &m_UIBoundsOverlay);
    ImGui::SameLine();
    ImGui::Checkbox("Clip overlay", &m_UIClipOverlay);
    ImGui::Checkbox("Hit order overlay", &m_UIHitOverlay);
    ImGui::Separator();
    auto selected = m_SceneHierarchyPanel.GetSelectedEntity();
    auto n = selected ? s.Find(selected.GetUUID()) : nullptr;
    if (n)
    {
        ImGui::Text("Selected %llu | canvas %llu | painter %zu", (unsigned long long)n->ID,
                    (unsigned long long)n->Canvas, n->Order);
        ImGui::Text("Visible %s | enabled %s | arranged %s", n->Visible ? "yes" : "no",
                    n->Enabled ? "yes" : "no", n->LayoutResolved ? "yes" : "no");
        ImGui::Text("Hit %s | focus eligible %s | container-owned %s", Policy(n->Hit),
                    n->FocusEligible ? "yes" : "no", n->ContainerOwned ? "yes" : "no");
        auto rect = [](const char *name, GameUI::Rect r) {
            ImGui::Text("%s: (%.2f, %.2f) %.2f x %.2f", name, r.Position.x, r.Position.y, r.Size.x,
                        r.Size.y);
        };
        rect("Logical bounds", n->Bounds);
        auto mode=[](GameUI::SizeMode value){return value==GameUI::SizeMode::Fixed?"Fixed":value==GameUI::SizeMode::Fill?"Fill":"Content";};
        ImGui::Text("Layout v%u: %s / %s | controlling parent %llu",GameUI::RectLayout::SchemaVersion,mode(n->WidthMode),mode(n->HeightMode),(unsigned long long)n->Parent);
        ImGui::Text("Available %.2f x %.2f | measured %.2f x %.2f",n->Available.x,n->Available.y,n->Measured.x,n->Measured.y);
        ImGui::Text("Arranged %.2f x %.2f | overflow %s",n->Bounds.Size.x,n->Bounds.Size.y,n->Overflow?"yes":"no");
        if(n->Line>=0){ImGui::Text("Wrap line %d",n->Line);rect("Allocated line",n->Slot);}
        if(n->GridRow>=0){ImGui::Text("Grid row %d column %d",n->GridRow,n->GridColumn);rect("Allocated cell",n->Slot);}
        GameUI::UIImageVisual image;if(m_ActiveScene->GetUIImageVisual(n->ID,image)){
            const char *fits[]={"Stretch","Contain","Cover","Native"};
            ImGui::Text("Last image paint: source %.1f x %.1f | %s | generation %llu",image.SourceSize.x,image.SourceSize.y,fits[int(image.Fit)],(unsigned long long)image.Generation);
            rect("Fitted image",image.Placement.Visual);rect("Source crop (normalized, top-left)",image.Placement.UV);
        }
        rect("Logical clip", n->Clip);
        rect("Pixel bounds", n->PixelBounds);
        rect("Effective pixel clip", n->PixelClip);
        rect("Framebuffer bounds", n->FramebufferBounds);
        rect("Framebuffer coverage", n->FramebufferClip);
    }
    else
        ImGui::TextDisabled("Select a Canvas or UI Rect to inspect geometry");
    if (!m_ActiveScene->GetUIError().empty())
        ImGui::TextWrapped("%s", m_ActiveScene->GetUIError().c_str());
    if (!m_UIEdit.Error().empty())
        ImGui::TextWrapped("Edit: %s", m_UIEdit.Error().c_str());
    if (!s.Warnings.empty() && ImGui::CollapsingHeader("Layout warnings"))
        for (auto &warning : s.Warnings)
            ImGui::TextWrapped("%llu: %s", (unsigned long long)warning.Entity,
                               warning.Message());
    ImGui::End();
}
} // namespace Hazel
