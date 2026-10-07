#include "AuthoringPanel.h"
#include "UI/PropertyUI.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/Framebuffer.h"
#include <imgui.h>
#include <algorithm>
#include <limits>
namespace Hazel {
void AuthoringPanel::PrefabPreview() {
    if (!m_PrefabScene || !ImGui::CollapsingHeader("Subtree preview"))
        return;
    try {
        const auto width = uint32_t(std::clamp(ImGui::GetContentRegionAvail().x, 96.f, 1024.f));
        const uint32_t height = 180;
        if (!m_PrefabPreview) {
            FramebufferSpecification specification;
            specification.Width = width;
            specification.Height = height;
            specification.Attachments = {FramebufferTextureFormat::RGBA8,
                                         FramebufferTextureFormat::RED_INTEGER,
                                         FramebufferTextureFormat::Depth};
            m_PrefabPreview = Framebuffer::Create(specification);
        } else if (m_PrefabPreview->GetSpecification().Width != width)
            m_PrefabPreview->Resize(width, height);
        if (ImGui::SmallButton("Fit Subtree"))
            m_PrefabFit = true;
        PropertyUI::Help("Frames transformed sprite bounds (unit bounds for other components). "
                         "Static preview; no scripts or physics run");
        m_PrefabScene->PrepareSprites();
        if (m_PrefabFit) {
            glm::vec3 minimum(std::numeric_limits<float>::max()),
                maximum(std::numeric_limits<float>::lowest());
            for (auto handle : m_PrefabScene->GetAllEntitiesWith<IDComponent>()) {
                Entity e(handle, m_PrefabScene.get());
                auto world = m_PrefabScene->GetWorldTransform(e);
                const auto *sprite = m_PrefabScene->RenderedSprite(e);
                for (size_t i = 0; i < 4; ++i) {
                    const glm::vec2 point =
                        sprite ? sprite->Corners[i]
                               : glm::vec2((i == 0 || i == 3) ? -.5f : .5f, i < 2 ? -.5f : .5f);
                    auto position = glm::vec3(world * glm::vec4(point, 0, 1));
                    minimum = glm::min(minimum, position);
                    maximum = glm::max(maximum, position);
                }
            }
            if (minimum.x <= maximum.x) {
                const auto span = maximum - minimum;
                m_PrefabCamera.RestoreOrbit(
                    minimum + (maximum - minimum) * .5f, 0, 0,
                    std::max(2.f, std::max(span.y, span.x / (float(width) / height)) * 1.6f));
            }
            m_PrefabFit = false;
        }
        m_PrefabCamera.SetViewportSize(float(width), float(height));
        {
            // Always release the offscreen target if scene rendering rejects a draft.
            struct PreviewBinding {
                Ref<Framebuffer> Buffer;
                explicit PreviewBinding(Ref<Framebuffer> buffer) : Buffer(std::move(buffer)) {
                    Buffer->Bind();
                }
                ~PreviewBinding() { Buffer->Unbind(); }
            } binding(m_PrefabPreview);
            RenderCommand::SetClearColor({.08f, .08f, .08f, 1});
            RenderCommand::Clear();
            m_PrefabPreview->ClearAttachment(1, -1);
            m_PrefabScene->OnUpdateEditor(0, m_PrefabCamera);
        }
        ImGui::Image(ImTextureID(uint64_t(m_PrefabPreview->GetColorAttachmentRendererID())),
                     {float(width), float(height)}, {0, 1}, {1, 0});
        ImGui::TextWrapped("Independent static preview. Fit again after changing local transforms; "
                           "long text may extend beyond unit bounds.");
    } catch (const std::exception &error) {
        ImGui::TextWrapped("Preview unavailable: %s. Authored data is retained.", error.what());
    }
}
} // namespace Hazel
