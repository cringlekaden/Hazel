#include "hzpch.h"
#include "Hazel/Scene/ScriptableEntity.h"
// Local ownership adaptation; target Bind<T>/component consumers remain intact.
namespace Hazel {
    NativeScriptComponent::NativeScriptComponent() = default;
    NativeScriptComponent::~NativeScriptComponent() = default;
    NativeScriptComponent::NativeScriptComponent(NativeScriptComponent&&) noexcept = default;
    NativeScriptComponent& NativeScriptComponent::operator=(NativeScriptComponent&&) noexcept = default;
    NativeScriptComponent::NativeScriptComponent(const NativeScriptComponent& other)
        : InstantiateScript(other.InstantiateScript), DestroyScript(other.DestroyScript) {}
    NativeScriptComponent& NativeScriptComponent::operator=(const NativeScriptComponent& other) {
        if (this!=&other) {
            Instance.reset();
            InstantiateScript=other.InstantiateScript;
            DestroyScript=other.DestroyScript;
        }
        return *this;
    }
}
