using System;
using System.Runtime.CompilerServices;

namespace Hazel
{
	public static class InternalCalls
	{
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void SpriteRendererComponent_SetSprite(ulong id,string sheet,ulong region);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void SpriteAnimationComponent_Play(ulong id,string sheet,ulong clip);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void SpriteAnimationComponent_Control(ulong id,int action);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool SpriteAnimationComponent_State(ulong id,bool finished);
		#region Entity
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong Hierarchy_GetParent(ulong id);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong[] Hierarchy_GetChildren(ulong id);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong Hierarchy_SetParent(ulong id, ulong parent, int mode);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static int Hierarchy_GetStatus(ulong scene, ulong request);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static string Hierarchy_GetReason(ulong scene, ulong request);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void Transform_GetLocal(ulong id, int axis, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void Transform_SetLocal(ulong id, int axis, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void Transform_GetWorldMatrix(ulong id, out Matrix4 value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong Entity_GetSceneIdentity();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool Entity_IsValid(ulong id, ulong scene);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void Entity_Destroy(ulong id, ulong scene);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong Entity_Instantiate(string path, ref Vector3 position, ref Vector3 rotation, ref Vector3 scale, bool replaceRotationAndScale);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Entity_FindEntityByName(string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static object GetScriptInstance(ulong entityID);
		#endregion

		#region TransformComponent
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void TransformComponent_GetScale(ulong id, out Vector3 scale);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void TransformComponent_SetScale(ulong id, ref Vector3 scale);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);
		#endregion

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_SetLinearVelocity(ulong entityID, ref Vector2 velocity);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static float CameraComponent_GetOrthographicSize(ulong entityID);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void CameraComponent_SetOrthographicSize(ulong entityID, float size);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static float CameraComponent_GetAspectRatio(ulong entityID);

		#region Rigidbody2DComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 linearVelocity);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static Rigidbody2DComponent.BodyType Rigidbody2DComponent_GetType(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetType(ulong entityID, Rigidbody2DComponent.BodyType type);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, ref Vector2 impulse, bool wake);
		#endregion

		#region TextComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string TextComponent_GetText(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetText(ulong entityID, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetColor(ulong entityID, ref Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float TextComponent_GetKerning(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetKerning(ulong entityID, float kerning);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float TextComponent_GetLineSpacing(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetLineSpacing(ulong entityID, float lineSpacing);
		#endregion


		#region Input
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyDown(KeyCode keycode);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsMouseButtonDown(MouseCode button);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Input_GetMouseWorldPosition(out Vector2 position);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void Scene_LoadScene(string assetPath);
		#endregion
	}
}
