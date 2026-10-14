#include "hzpch.h"
#include "Entity.h"
#include "Hazel/Project/Project.h"
#include "Hazel/UI/RuntimeUI.h"
#include "Hazel/UI/RuntimeUIConfig.h"
#include "Scene.h"
#include "Hazel/Utils/UTF8.h"
namespace Hazel
{
const std::vector<UUID> &Scene::GetChildrenView(UUID parent) const
{
    CheckThread();
    static const std::vector<UUID> empty;
    auto it = m_Children.find(parent);
    return it == m_Children.end() ? empty : it->second;
}
void Scene::RefreshUI()
{
    glm::vec2 viewport{m_ViewportWidth,m_ViewportHeight};
    m_UI->ResolveSurface(GameUI::SurfaceMetrics{viewport,m_UIFramebufferExplicit?m_UIFramebufferSize:viewport}, !m_IsRunning);
}
void Scene::UpdateUIInput(const GameUI::InputFrame &frame)
{
    RefreshUI();
    m_UI->UpdateInput(frame);
}
void Scene::RenderUI()
{
    try
    {
        RefreshUI();
        m_UI->Draw();
        m_UIError.clear();
    }
    catch (const std::exception &error)
    {
        if (m_IsRunning)
            throw;
        m_UIError = error.what();
    }
}
bool Scene::ConsumeUIAction(uint64_t entity, GameUI::Action &action)
{
    CheckThread();
    return m_UI->Consume(entity, action);
}
bool Scene::IsGameplayInputBlocked() const
{
    return m_UI->BlocksGameplay();
}
bool Scene::IsUICapturingPointer() const {return m_UI->CapturesPointer();}
bool Scene::IsGameplayPointerBlocked() const
{
    return m_UI->BlocksPointer();
}
bool Scene::GetUIBounds(uint64_t entity, GameUI::Rect &pixels) const
{
    return m_UI->Bounds(entity, pixels);
}
void Scene::ResetUI()
{
    m_UI->Reset();
}
void Scene::RetireUI(uint64_t id)
{
    m_UI->Retire(id);
}
void Scene::RetireUISubtree(uint64_t id)
{
    auto e=GetEntityByUUID(id);
    if(e && IsEntityValid(id))for(auto child:GetSubtree(e))m_UI->Retire(child);
    else m_UI->Retire(id);
}
void Scene::CancelUIInput() { CheckThread();m_UI->CancelInput(); }
void Scene::SetUIFramebufferSize(glm::vec2 size)
{
    CheckThread();m_UIFramebufferSize=size;m_UIFramebufferExplicit=true;
}
const GameUI::ResolvedSnapshot& Scene::GetUISnapshot() const { CheckThread();return m_UI->Snapshot(); }
GameUI::RuntimeUIDiagnostics Scene::GetUIDiagnostics() const { CheckThread();return m_UI->Diagnostics(); }
void Scene::ValidateUI(bool resources)
{
    if(m_Registry.view<UIRectComponent>().size()>GameUI::InputRouter::MaxButtons)
        throw std::runtime_error("UI declared rect limit exceeded (4096, including hidden rects)");
    for (auto h : m_Registry.view<IDComponent>())
    {
        Entity e(h, this);
        if (!IsEntityValid(e.GetUUID()))
            continue;
        if (!e.HasComponent<UICanvasComponent>() && !e.HasComponent<UIRectComponent>() &&
            !e.HasComponent<UILayoutComponent>() && !e.HasComponent<UITextStyleComponent>() &&
            !e.HasComponent<UIButtonComponent>() && !e.HasComponent<UINineSliceComponent>())
            continue;
        ValidateUIConfiguration(YAML::Load(UIAuthoredState(e)));
        if(e.HasComponent<TextComponent>() && e.HasComponent<UIRectComponent>())
        {
            const auto& text=e.GetComponent<TextComponent>().TextString;
            if(text.size()>65536)throw std::invalid_argument("UI text exceeds 64 KiB");
            UTF8::Validate(text);
        }
        if (e.HasComponent<UITextStyleComponent>())
        {
            if (!e.HasComponent<TextComponent>() || !e.HasComponent<UIRectComponent>())
                throw std::runtime_error("UI text style needs Text and UI Rect");
            auto path = e.GetComponent<UITextStyleComponent>().FontPath;
            if (!path.empty())
            {
                if (!m_Assets)
                    throw std::runtime_error("UI font needs project assets");
                Font::ValidateFile(Project::ResolveOwnedAsset(m_Assets->Root(), path));
                if (resources)
                    m_Assets->FontAsset(path);
            }
        }
        auto validateSource = [&](const SpriteSource &source) {
            if (!m_Assets)
                return;
            if (auto image = std::get_if<TextureSpriteSource>(&source);
                image && !image->Texture.empty())
                Project::ResolveOwnedAsset(m_Assets->Root(), image->Texture);
            if (auto image = std::get_if<SpriteReference>(&source); image && image->Region)
                Project::ResolveOwnedAsset(m_Assets->Root(), image->Sheet);
        };
        if (e.HasComponent<UIRectComponent>() && e.HasComponent<SpriteRendererComponent>())
            validateSource(e.GetComponent<SpriteRendererComponent>().Source);
        if (e.HasComponent<UIButtonComponent>())
        {
            if (!e.HasComponent<UIRectComponent>())
                throw std::runtime_error("UI button needs UI Rect");
            for (auto &source : e.GetComponent<UIButtonComponent>().Images)
            {
                validateSource(source);
                if (resources && m_Assets && source.index())
                    m_Assets->Resolve(source);
            }
        }
        if (e.HasComponent<UINineSliceComponent>())
        {
            if (!e.HasComponent<SpriteRendererComponent>() || !e.HasComponent<UIRectComponent>())
                throw std::runtime_error("Nine-slice needs UI Rect and Sprite Renderer");
            if (resources && m_Assets)
            {
                auto validate = [&](const SpriteSource &source) {
                    auto sprite = m_Assets->Resolve(source);
                    auto borders = e.GetComponent<UINineSliceComponent>().Borders;
                    if (!sprite->Texture)
                        throw std::runtime_error("Nine-slice needs an image");
                    auto size =
                        glm::abs(sprite->UV[2] - sprite->UV[0]) *
                        glm::vec2(sprite->Texture->GetWidth(), sprite->Texture->GetHeight());
                    if (borders.x + borders.z > size.x || borders.y + borders.w > size.y)
                        throw std::runtime_error("Nine-slice borders exceed source");
                };
                validate(e.GetComponent<SpriteRendererComponent>().Source);
                if (e.HasComponent<UIButtonComponent>())
                    for (auto &source : e.GetComponent<UIButtonComponent>().Images)
                        if (source.index())
                            validate(source);
            }
        }
    }
}
} // namespace Hazel
