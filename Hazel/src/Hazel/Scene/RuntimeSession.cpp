#include "hzpch.h"
#include "RuntimeSession.h"
#include "Scene.h"
#include "Entity.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>

namespace Hazel {
    static uint64_t s_NextSession = 0; // Main thread only.
    RuntimeSession::RuntimeSession() : m_Thread(std::this_thread::get_id()) {}
    RuntimeSession::~RuntimeSession() { Stop(); }
    void RuntimeSession::CheckThread() const {
        if (std::this_thread::get_id() != m_Thread) throw std::logic_error("RuntimeSession requires its main thread");
    }
    void RuntimeSession::Validate(const Ref<Scene>& scene) const {
        if (!scene) throw std::runtime_error("No scene to run");
        // Validate Box2D preconditions while the old scene is still usable.
        for (auto handle : scene->GetAllEntitiesWith<TransformComponent, Rigidbody2DComponent>()) {
            Entity entity(handle, scene.get());
            const auto type = entity.GetComponent<Rigidbody2DComponent>().Type;
            if (type != Rigidbody2DComponent::BodyType::Static && type != Rigidbody2DComponent::BodyType::Dynamic && type != Rigidbody2DComponent::BodyType::Kinematic)
                throw std::runtime_error("Invalid physics body type: " + entity.GetName());
            if (!std::isfinite(entity.GetComponent<Rigidbody2DComponent>().GravityScale))
                throw std::runtime_error("Non-finite gravity scale: " + entity.GetName());
            const auto& transform = entity.GetComponent<TransformComponent>();
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(transform.Translation[axis]) || !std::isfinite(transform.Rotation[axis]) || !std::isfinite(transform.Scale[axis]))
                    throw std::runtime_error("Non-finite physics transform: " + entity.GetName());
            auto material = [&](float density, float friction, float restitution, float threshold) {
                if (!std::isfinite(density) || !std::isfinite(friction) || !std::isfinite(restitution) || !std::isfinite(threshold) ||
                    density < 0 || friction < 0 || restitution < 0 || threshold < 0)
                    throw std::runtime_error("Invalid physics material: " + entity.GetName());
            };
            if (entity.HasComponent<BoxCollider2DComponent>()) {
                const auto& box = entity.GetComponent<BoxCollider2DComponent>();
                material(box.Density, box.Friction, box.Restitution, box.RestitutionThreshold);
                if (!std::isfinite(box.Size.x) || !std::isfinite(box.Size.y) || !std::isfinite(box.Offset.x) || !std::isfinite(box.Offset.y) ||
                    !std::isfinite(box.Size.x*transform.Scale.x) || !std::isfinite(box.Size.y*transform.Scale.y) || box.Size.x*transform.Scale.x <= 0 || box.Size.y*transform.Scale.y <= 0)
                    throw std::runtime_error("Invalid box collider size/offset: " + entity.GetName());
            }
            if (entity.HasComponent<CircleCollider2DComponent>()) {
                const auto& circle = entity.GetComponent<CircleCollider2DComponent>();
                material(circle.Density, circle.Friction, circle.Restitution, circle.RestitutionThreshold);
                if (!std::isfinite(circle.Radius) || !std::isfinite(circle.Offset.x) || !std::isfinite(circle.Offset.y) || !std::isfinite(circle.Radius*transform.Scale.x) || circle.Radius*transform.Scale.x <= 0)
                    throw std::runtime_error("Invalid circle collider radius/offset: " + entity.GetName());
            }
        }
        for (auto handle : scene->GetAllEntitiesWith<ScriptComponent>()) {
            const auto& script = Entity(handle, scene.get()).GetComponent<ScriptComponent>();
            if (!ScriptEngine::EntityClassExists(script.ClassName))
                throw std::runtime_error("Runtime script class is unavailable: " + script.ClassName);
        }
    }
    void RuntimeSession::Start(const Ref<Project>& project, const Ref<Scene>& authoredScene) {
        CheckThread();
        if (m_Updating || m_Stopping) throw std::logic_error("Cannot start a session inside its update/stop");
        Validate(authoredScene);
        auto scene = Scene::Copy(authoredScene);
        if (auto* current = ScriptEngine::GetRuntimeSession(); current && current != this)
            throw std::logic_error("Only one runtime session may use the script environment");
        Stop();
        m_Project = project; m_Scene = std::move(scene);
        m_Identity = ++s_NextSession; m_Error.clear();
        m_Scene->OnViewportResize(m_Width, m_Height);
        ScriptEngine::SetRuntimeSession(this);
        try { m_Scene->OnRuntimeStart(); }
        catch (...) { Stop(); throw; }
    }
    void RuntimeSession::Stop() {
        CheckThread();
        if (m_Stopping) return;
        if (m_Updating) throw std::logic_error("Cannot stop a scene inside its callback; request a transition instead");
        m_Stopping = true;
        m_Pending.reset(); m_Identity = 0; m_InputEnabled = false;
        // Unbind before native OnDestroy, so retired callbacks cannot queue work.
        if (ScriptEngine::GetRuntimeSession() == this) ScriptEngine::SetRuntimeSession(nullptr);
        if (m_Scene) m_Scene->OnRuntimeStop();
        m_Scene.reset(); m_Project.reset(); m_Stopping = false;
    }
    bool RuntimeSession::RequestSceneLoad(const std::filesystem::path& reference) {
        CheckThread();
        if (!m_Scene || m_Stopping || !m_Project) return false;
        const auto normalized = Project::NormalizeAssetPath(reference).lexically_normal();
        const auto text = normalized.generic_u8string();
        if (normalized.empty() || normalized.has_root_path() || *normalized.begin() == ".." ||
            (text.size() > 1 && text[1] == ':') || normalized.extension() != ".hazel") {
            HZ_CORE_ERROR("Scene.LoadScene requires an asset-relative .hazel path: {}", text); return false;
        }
        if (m_Pending) {
            if (*m_Pending == normalized) return true;
            HZ_CORE_WARN("Scene transition already requested; ignoring '{}' (first request wins)", text); return false;
        }
        m_Pending = normalized;
        return true;
    }
    void RuntimeSession::CommitPendingTransition() {
        if (!m_Pending) return;
        auto reference = *m_Pending; m_Pending.reset();
        Ref<Scene> candidate;
        try {
            candidate = m_Project->LoadScene(reference);
            Validate(candidate);
            candidate->OnViewportResize(m_Width, m_Height);
        } catch (const std::runtime_error& error) {
            m_Error = "Scene transition '" + reference.generic_u8string() + "': " + error.what();
            HZ_CORE_ERROR("{}", m_Error); return;
        }
        // All callbacks/registry walks have returned. Retire observations before
        // starting the target, with the same domain and assembly watcher alive.
        m_Stopping = true;
        ScriptEngine::SetRuntimeSession(nullptr);
        m_Scene->OnRuntimeStop();
        auto previous = std::move(m_Scene);
        m_Scene = std::move(candidate); m_Stopping = false;
        ScriptEngine::SetRuntimeSession(this);
        try { m_Scene->OnRuntimeStart(); m_Error.clear(); HZ_CORE_INFO("Runtime scene: {}", reference.generic_u8string()); }
        catch (const std::runtime_error& error) {
            m_Pending.reset(); m_Scene->OnRuntimeStop(); m_Scene = std::move(previous);
            m_Scene->OnRuntimeStart();
            m_Error = "Cannot start target scene: " + std::string(error.what()); HZ_CORE_ERROR("{}", m_Error);
        }
    }
    void RuntimeSession::Update(Timestep timestep) {
        CheckThread();
        if (!m_Scene) return;
        if (m_Updating) throw std::logic_error("Reentrant runtime update");
        CommitPendingTransition(); // One transition at most; target OnCreate waits one more frame.
        m_Updating = true;
        try { m_Scene->OnUpdateRuntime(timestep); m_Updating = false; }
        catch (...) { m_Updating = false; throw; }
    }
    void RuntimeSession::Resize(uint32_t width, uint32_t height) {
        CheckThread(); m_Width = width; m_Height = height;
        if (m_Scene) m_Scene->OnViewportResize(width, height);
    }
    void RuntimeSession::SetInput(glm::vec2 position, bool enabled) {
        CheckThread(); m_Mouse = position; m_InputEnabled = enabled;
    }
    bool RuntimeSession::GetMouseWorldPosition(glm::vec2& position) const {
        if (!m_Scene || !m_InputEnabled || !m_Width || !m_Height || m_Mouse.x < 0 || m_Mouse.y < 0 || m_Mouse.x >= m_Width || m_Mouse.y >= m_Height) return false;
        auto camera = m_Scene->GetPrimaryCameraEntity();
        if (!camera) return false;
        const auto transform = camera.GetComponent<TransformComponent>().GetTransform();
        const auto viewProjection = camera.GetComponent<CameraComponent>().Camera.GetProjection() * glm::inverse(transform);
        const auto inverse = glm::inverse(viewProjection);
        glm::vec4 rayNear = inverse * glm::vec4(2*m_Mouse.x/m_Width-1, 1-2*m_Mouse.y/m_Height, -1, 1);
        glm::vec4 rayFar = inverse * glm::vec4(2*m_Mouse.x/m_Width-1, 1-2*m_Mouse.y/m_Height, 1, 1);
        if (std::abs(rayNear.w)<1e-7f || std::abs(rayFar.w)<1e-7f) return false;
        rayNear /= rayNear.w; rayFar /= rayFar.w;
        const auto ray = rayFar-rayNear;
        if (std::abs(ray.z)<1e-7f) return false;
        const float distance = -rayNear.z/ray.z; // Example interaction is on world z=0.
        position = glm::vec2(rayNear+distance*ray);
        return std::isfinite(position.x) && std::isfinite(position.y) && distance >= 0 && distance <= 1;
    }
}
