using System;
using System.Runtime.CompilerServices;

namespace Hazel
{
	public static class InternalCalls
    {
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_Binding(int action,ref int key,ref int alternate,ref int pad,ref int shift,bool write);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_InputSettings(ref UIInputSettings settings,bool write);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_ResetBindings();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static float Runtime_AudioVolume(float value,bool write);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void Runtime_Exit();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong UI_FocusScope();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Focus(ulong identity,ulong target);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Checked(ulong id,bool write,bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong UI_Selection(ulong id,bool write,ulong value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static float UI_Progress(ulong id,bool write,float value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static ulong UI_PresentationReference(ulong id,int kind);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Dropdown(ulong id,int operation);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_ScrollInfo(ulong id,out Vector2 offset,out Vector2 range,out Vector2 viewport,out Vector2 extent);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_ScrollTo(ulong id,ref Vector2 offset,bool normalized);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_EnsureVisible(ulong id,ulong target,bool focus);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static float UI_Slider(ulong id,bool write,float value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Screen(ulong identity,ulong screen,int operation);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_TextInputOwned();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_Style(ulong entity,bool write,string asset,string variant,bool enabled,out string actualAsset,out string actualVariant,out bool actualEnabled);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_StyleReset(ulong entity,uint groups);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static uint UI_StyleOverrides(ulong entity,uint groups,bool enable);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static string UI_TextEntry(ulong id,bool write,string value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_ConsumeEvent(ulong identity,out ulong screen,out ulong source,out int kind,out ulong sequence,out string action,out bool previousChecked,out bool checkedValue,out ulong previousSelection,out ulong selection,out ulong owner,out float previousValue,out float value,out float startValue,out ulong gesture,out bool interrupted,out string previousText,out string text);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Cancel(ulong identity,ulong scope);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_GetImageFit(ulong id,out int fit,out Vector2 alignment);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_SetImageFit(ulong id,int fit,ref Vector2 alignment);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_GetLayout(ulong id,out UILayoutSettings settings);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static int UI_TrackCount(ulong id,bool rows);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_GetTrack(ulong id,bool rows,int index,out UITrack track);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_SetLayout(ulong id,ref UILayoutSettings settings,UITrack[] columns,UITrack[] rows);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_GetCell(ulong id,out int row,out int column,out int rowSpan,out int columnSpan);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_SetCell(ulong id,int row,int column,int rowSpan,int columnSpan);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_IsPointerCaptured();
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_IsKeyDown(KeyCode key);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_Consume(ulong scene,ulong id,out ulong entity,out string action);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static bool UI_GetValue(ulong id,int property);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_SetValue(ulong id,int property,bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static uint UI_LayoutVersion(ulong id);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_TextOverflow(ulong id,ref int mode,ref uint lines,bool write);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_GetSizing(ulong id,int axis,out int mode,out float size,out float minimum,out int hasMaximum,out float maximum);
        [MethodImpl(MethodImplOptions.InternalCall)] internal extern static void UI_SetSizing(ulong id,int axis,int mode,float size,float minimum,int hasMaximum,float maximum);

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
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool SaveData_IsPersistent();
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern bool AudioSourceComponent_Play(ulong entity);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void AudioSourceComponent_Stop(ulong entity);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern string SaveData_Read(string slot);
        [MethodImpl(MethodImplOptions.InternalCall)] internal static extern void SaveData_Write(string slot, string payload);
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
        internal extern static void TextComponent_SetRichText(ulong entityID, string source, TextEmphasis[] spans);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static int TextComponent_GetEmphasisCount(ulong entityID);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void TextComponent_GetEmphasis(ulong entityID, TextEmphasis[] spans);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static string TextComponent_GetFontFamily(ulong entityID);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static int TextComponent_GetFontStyle(ulong entityID);
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static void TextComponent_SetFont(ulong entityID, string family, int style);

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
