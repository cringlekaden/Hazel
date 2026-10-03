// Exercises the actual published Hazel managed API in native embedding tests.
using System;
namespace Migration {
    public class Probe : Hazel.Entity {
        public float Speed = 2.5f;
        public char Character = '\u00e9';
        public ulong Unsigned = 0xfedcba9876543210UL;
        public Hazel.Vector3 Position = new Hazel.Vector3(1, 2, 3);
        public float Evaluate(float timestep) {
            Position = Position + new Hazel.Vector3(Speed * timestep);
            return Position.X + Position.Y + Position.Z;
        }
        public static void ThrowManaged() { throw new InvalidOperationException("Managed fixture exception"); }
    }
    public class SceneProbe : Hazel.Entity {
        public float Speed = 2.5f;
        public double Precise = 1.25;
        public bool Enabled = true;
        public char Character = '\u03bb';
        public sbyte SignedByte = -12;
        public byte UnsignedByte = 250;
        public short SignedShort = -1234;
        public ushort UnsignedShort = 60000;
        public int SignedInt = -123456;
        public uint UnsignedInt = 4000000000U;
        public long SignedLong = -1234567890123L;
        public ulong UnsignedLong = 0xfedcba9876543210UL;
        public Hazel.Vector2 Pair = new Hazel.Vector2(2, 3);
        public Hazel.Vector3 Position = new Hazel.Vector3(1, 2, 3);
        public Hazel.Vector4 Color = new Hazel.Vector4(1, 2, 3, 4);
        public Hazel.Entity Target;
        public int Creates;
        public int Updates;
        public bool HasTransform;
        public bool HasBody;
        public bool HasText;
        protected int ProtectedField = 17;
        private int PrivateField = 19;
        public static int StaticField = 23;
        void OnCreate() {
            Creates++;
            HasTransform = HasComponent<Hazel.TransformComponent>();
            HasBody = HasComponent<Hazel.Rigidbody2DComponent>();
            HasText = HasComponent<Hazel.TextComponent>();
            if (HasBody) GetComponent<Hazel.Rigidbody2DComponent>().ApplyLinearImpulse(new Hazel.Vector2(1, 0), true);
        }
        void OnUpdate(float timestep) {
            Updates++;
            if (!HasBody) Translation = Translation + new Hazel.Vector3(Speed * timestep, 0, 0);
        }
    }
    // Local regression fixture for the actual public component/internal-call API.
    public class ComponentProbe : Hazel.Entity {
        public bool Passed;
        void OnCreate() {
            var transform = GetComponent<Hazel.TransformComponent>();
            transform.Translation = new Hazel.Vector3(3, 4, 0);
            if (transform.Translation.X != 3 || transform.Translation.Y != 4) throw new Exception("Transform calls");
            var text = GetComponent<Hazel.TextComponent>();
            text.Text = "Hazel é λ";
            text.Color = new Hazel.Vector4(0.1f, 0.2f, 0.3f, 0.4f);
            text.Kerning = 0.25f; text.LineSpacing = 0.5f;
            if (text.Text != "Hazel é λ" || text.Color.W != 0.4f || text.Kerning != 0.25f || text.LineSpacing != 0.5f) throw new Exception("Text calls");
            var body = GetComponent<Hazel.Rigidbody2DComponent>();
            foreach (var type in new[] { Hazel.Rigidbody2DComponent.BodyType.Static, Hazel.Rigidbody2DComponent.BodyType.Kinematic, Hazel.Rigidbody2DComponent.BodyType.Dynamic }) {
                body.Type = type; if (body.Type != type) throw new Exception("Body type calls");
            }
            body.ApplyLinearImpulse(new Hazel.Vector2(1, 0), new Hazel.Vector2(0, 0), true);
            body.ApplyLinearImpulse(new Hazel.Vector2(1, 0), true);
            if (body.LinearVelocity.X <= 0) throw new Exception("Velocity/impulse calls");
            Hazel.Input.IsKeyDown(Hazel.KeyCode.Space);
            if (FindEntityByName("Player") == null || FindEntityByName("Missing") != null) throw new Exception("Entity calls");
            Passed = true;
        }
    }
}

namespace Migration {
    public class TransitionOnCreate : Hazel.Entity {
        public int Creates;
        void OnCreate() { Creates++; Hazel.Scene.LoadScene("Scenes/Target.hazel"); }
    }
    public class TransitionOnUpdate : Hazel.Entity {
        public int Updates;
        void OnUpdate(float timestep) {
            Updates++;
            Hazel.Scene.LoadScene("Scenes/Target.hazel");
            Hazel.Scene.LoadScene("Scenes/Conflicting.hazel");
        }
    }
}
