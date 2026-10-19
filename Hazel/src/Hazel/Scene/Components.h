#pragma once
// Actual target component shapes; explicit public-header prerequisites.
#include "Hazel/Core/Base.h"
#include <string>

#include "SceneCamera.h"
#include "Hazel/UI/RuntimeUIModel.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Renderer/Font.h"
#include "Hazel/Assets/ProjectAssets.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>

namespace Hazel {

	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		SpriteSource Source;
		SpriteResolution Resolved;
		SpriteSource PreparedSource;
		uint64_t PreparedEpoch = 0;
		// Explicit bridge for procedural C++ drawing and legacy callers. File identity is captured once.
		void SetTexture(const Ref<Texture2D>& texture, float tiling = 1) {
			Source = texture ? SpriteSource(TextureSpriteSource{std::filesystem::u8path(texture->GetPath()),tiling,texture}) : SpriteSource(std::monostate{});
			auto draw=CreateRef<ResolvedSprite>();draw->Texture=texture;draw->TilingFactor=tiling;Resolved={draw,{}};PreparedSource=Source;PreparedEpoch=0;
		}

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {}
	};
	struct SpriteAnimationComponent : SpriteAnimationSettings {
		// Independent per-entity transient state; copies reset this in Scene::CopyForScene.
		AnimationReference Current;
		SpritePlayback Playback;
		Ref<const ResolvedClip> Resolved;
		bool Initialized = false;
		uint64_t PreparedEpoch = 0;
		std::string Error;
		void ResetRuntime() { Current={};Playback.Reset();Resolved.reset();Initialized=false;PreparedEpoch=0;Error.clear(); }
	};

    struct AudioSourceComponent {
        std::filesystem::path Clip;
        float Gain=0.5f;
        bool Loop=false, PlayOnStart=false;
    };

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true; // TODO: think about moving to Scene
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
	};

	// Forward declaration
	class ScriptableEntity;

	struct NativeScriptComponent
	{
		Scope<ScriptableEntity> Instance;

		Scope<ScriptableEntity>(*InstantiateScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		NativeScriptComponent();
		~NativeScriptComponent();
		NativeScriptComponent(const NativeScriptComponent&);
		NativeScriptComponent& operator=(const NativeScriptComponent&);
		NativeScriptComponent(NativeScriptComponent&&) noexcept;
		NativeScriptComponent& operator=(NativeScriptComponent&&) noexcept;

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() -> Scope<ScriptableEntity> { return CreateScope<T>(); };
			DestroyScript = [](NativeScriptComponent* nsc) { nsc->Instance.reset(); };
		}
	};

	// Physics

	struct Rigidbody2DComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;
		float GravityScale = 1.0f;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		// TODO(Yan): move into physics material in the future maybe
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 0.5f;

		// TODO(Yan): move into physics material in the future maybe
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	struct TextComponent
	{
		std::string TextString;
		Ref<Font> FontAsset = Font::GetDefault();
		glm::vec4 Color{ 1.0f };
		float Kerning = 0.0f;
		float LineSpacing = 0.0f;
	};


    // Authored logical pixels, top-left origin. Runtime resources/state live in Scene.
    struct UICanvasComponent : GameUI::CanvasLayout {
        int SortOrder=0;
        bool Visible=true, Enabled=true, Modal=false;
    };
    struct UIRectComponent : GameUI::RectLayout {
        bool Visible=true, Enabled=true, ClipChildren=true;
        float Flex=0;
        GameUI::ImageFit Fit=GameUI::ImageFit::Stretch;
        glm::vec2 ImageAlignment{.5f,.5f};
    };
    struct UILayoutComponent : GameUI::FlowLayout {};
    struct UITextStyleComponent {
        std::filesystem::path FontPath; // empty selects the packaged default font
        float Size=24;
        GameUI::Align Horizontal=GameUI::Align::Start, Vertical=GameUI::Align::Start;
        bool Wrap=true;
    };
    struct UINineSliceComponent {
        glm::vec4 Borders{4}; // source pixels: left, top, right, bottom
        float BorderScale=1;
    };
    struct UIButtonComponent {
        std::string Action="activate";
        int TabOrder=0;
        bool Interactable=true;
        std::array<glm::vec4,5> Colors{{glm::vec4(1),glm::vec4(1.15f,1.15f,1.15f,1),glm::vec4(.75f,.75f,.75f,1),glm::vec4(.5f),glm::vec4(1.2f,1.15f,.8f,1)}};
        std::array<SpriteSource,5> Images;
    };

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using AllComponents =
		ComponentGroup<TransformComponent, SpriteRendererComponent, SpriteAnimationComponent, AudioSourceComponent,
			CircleRendererComponent, CameraComponent, ScriptComponent,
			NativeScriptComponent, Rigidbody2DComponent, BoxCollider2DComponent,
			CircleCollider2DComponent, TextComponent, UICanvasComponent, UIRectComponent, UILayoutComponent, UITextStyleComponent, UINineSliceComponent, UIButtonComponent>;

}
