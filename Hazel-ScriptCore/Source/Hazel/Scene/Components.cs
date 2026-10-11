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

	public class TextComponent : Component
	{

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
