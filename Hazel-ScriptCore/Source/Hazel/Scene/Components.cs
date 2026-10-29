using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Hazel
{
	public abstract class Component
	{
		public Entity Entity { get; internal set; }
	}

	public class TransformComponent : Component
	{
		public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(Entity.CheckedID, out Vector3 translation);
				return translation;
			}
			set
			{
				InternalCalls.TransformComponent_SetTranslation(Entity.CheckedID, ref value);
			}
		}
	}

	public class Rigidbody2DComponent : Component
	{
		public enum BodyType { Static = 0, Dynamic, Kinematic }

		public Vector2 LinearVelocity
		{
			get
			{
				InternalCalls.Rigidbody2DComponent_GetLinearVelocity(Entity.CheckedID, out Vector2 velocity);
				return velocity;
			}
            set => InternalCalls.Rigidbody2DComponent_SetLinearVelocity(Entity.CheckedID, ref value);
		}

		public BodyType Type
		{
			get => InternalCalls.Rigidbody2DComponent_GetType(Entity.CheckedID);
			set => InternalCalls.Rigidbody2DComponent_SetType(Entity.CheckedID, value);
		}

		public void ApplyLinearImpulse(Vector2 impulse, Vector2 worldPosition, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.CheckedID, ref impulse, ref worldPosition, wake);
		}

		public void ApplyLinearImpulse(Vector2 impulse, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyLinearImpulseToCenter(Entity.CheckedID, ref impulse, wake);
		}

	}

    // The viewport supplies AspectRatio; scripts may fit an authored play area.
    public class CameraComponent : Component
    {
        public float OrthographicSize
        {
            get => InternalCalls.CameraComponent_GetOrthographicSize(Entity.CheckedID);
            set => InternalCalls.CameraComponent_SetOrthographicSize(Entity.CheckedID, value);
        }
        public float AspectRatio => InternalCalls.CameraComponent_GetAspectRatio(Entity.CheckedID);
    }

	public class AudioSourceComponent : Component
    {
        public bool Play() => InternalCalls.AudioSourceComponent_Play(Entity.CheckedID);
        public void Stop() => InternalCalls.AudioSourceComponent_Stop(Entity.CheckedID);
    }

    public enum FontStyle { Regular, Bold, Italic, BoldItalic }

    // Half-open UTF-16 endpoints (C# string indices), on complete graphemes and
    // shaped clusters. Null attributes inherit; ranges are sorted/nonoverlapping.
    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    public struct TextEmphasis {
        public uint Start, End;
        private uint Flags;
        private FontStyle FaceValue;
        private Vector4 ColorValue;
        public FontStyle? Face { get { return (Flags & 1) != 0 ? (FontStyle?)FaceValue : null; } }
        public Vector4? Color { get { return (Flags & 2) != 0 ? (Vector4?)ColorValue : null; } }
        public TextEmphasis(uint start, uint end, FontStyle? face = null, Vector4? color = null) {
            Start=start; End=end; Flags=(face.HasValue?1u:0u)|(color.HasValue?2u:0u);
            FaceValue=face ?? FontStyle.Regular; ColorValue=color ?? new Vector4(1);
        }
    }

	public class TextComponent : Component
	{

        // Replaces source and emphasis together, or throws without publishing.
        // Ordinary Text replacement clears display emphasis. Text entry is plain.
        public void SetRichText(string source, params TextEmphasis[] spans) =>
            InternalCalls.TextComponent_SetRichText(Entity.CheckedID, source ?? "", spans ?? new TextEmphasis[0]);
        public TextEmphasis[] Emphasis {
            get { var spans=new TextEmphasis[InternalCalls.TextComponent_GetEmphasisCount(Entity.CheckedID)];
                InternalCalls.TextComponent_GetEmphasis(Entity.CheckedID, spans); return spans; }
        }
        public string FontFamily {
            get => InternalCalls.TextComponent_GetFontFamily(Entity.CheckedID);
            set => SetFont(value, Style);
        }
        public FontStyle Style {
            get => (FontStyle)InternalCalls.TextComponent_GetFontStyle(Entity.CheckedID);
            set => SetFont(FontFamily, value);
        }
        // Empty family uses packaged engine Open Sans; absent optional faces use Regular.
        public void SetFont(string family, FontStyle style = FontStyle.Regular) =>
            InternalCalls.TextComponent_SetFont(Entity.CheckedID, family ?? "", (int)style);

		public string Text
		{
			get => InternalCalls.TextComponent_GetText(Entity.CheckedID);
			set => InternalCalls.TextComponent_SetText(Entity.CheckedID, value);
		}

		public Vector4 Color
		{
			get
			{
				InternalCalls.TextComponent_GetColor(Entity.CheckedID, out Vector4 color);
				return color;
			}

			set
			{
				InternalCalls.TextComponent_SetColor(Entity.CheckedID, ref value);
			}
		}

		public float Kerning
		{
			get => InternalCalls.TextComponent_GetKerning(Entity.CheckedID);
			set => InternalCalls.TextComponent_SetKerning(Entity.CheckedID, value);
		}

		public float LineSpacing
		{
			get => InternalCalls.TextComponent_GetLineSpacing(Entity.CheckedID);
			set => InternalCalls.TextComponent_SetLineSpacing(Entity.CheckedID, value);
		}

	}

}
