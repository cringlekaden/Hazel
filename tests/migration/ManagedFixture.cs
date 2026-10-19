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

namespace Migration {
    public class MotionCameraProbe : Hazel.Entity {
        public bool Passed;
        void OnCreate() {
            var body = GetComponent<Hazel.Rigidbody2DComponent>();
            body.LinearVelocity = new Hazel.Vector2(2, 0);
            Translation = new Hazel.Vector3(3, 4, 0);
            var camera = FindEntityByName("Camera").GetComponent<Hazel.CameraComponent>();
            camera.OrthographicSize = 12;
            Passed = body.LinearVelocity.X == 2 && camera.OrthographicSize == 12 && camera.AspectRatio == 2;
        }
    }
}
namespace Migration {
    public class LifecycleChild : Hazel.Entity {
        public int Creates, Updates;
        public float InitialX, InitialVelocityX;
        public bool BodyReady;
        void OnCreate() { Creates++;InitialX=Translation.X;var body=GetComponent<Hazel.Rigidbody2DComponent>();InitialVelocityX=body.LinearVelocity.X;BodyReady=true; }
        void OnUpdate(float dt) { Updates++; }
        void OnDestroy() { System.Console.WriteLine("LIFECYCLE: child destroyed"); }
    }
    public class LifecycleSpawner : Hazel.Entity {
        public Hazel.Prefab Child;
        public int Updates;
        public bool InvalidatedImmediately;
        private Hazel.Entity spawned;
        void OnUpdate(float dt) {
            Updates++;
            if(Updates==1) {
                spawned=Hazel.Entity.Instantiate(Child,new Hazel.Vector3(7,3,0));
                spawned.GetComponent<Hazel.Rigidbody2DComponent>().LinearVelocity=new Hazel.Vector2(1,0);
            }
            if(Updates==2) { spawned.Destroy();spawned.Destroy();InvalidatedImmediately=!spawned.IsValid; }
            if(Updates==3) { Destroy();Destroy(); }
        }
        void OnDestroy() { System.Console.WriteLine("LIFECYCLE: spawner destroyed"); }
    }
}
namespace Migration {
    public class SpriteProbe : Hazel.Entity {
        public Hazel.Sprite Icon;
        public Hazel.SpriteAnimation Clip;
        public bool Passed, Finished;
        void OnCreate() {
            GetComponent<Hazel.SpriteRendererComponent>().SetSprite(Icon);
            var animation=GetComponent<Hazel.SpriteAnimationComponent>();
            animation.Play(Clip);animation.Pause();
            if(animation.IsPlaying)throw new System.Exception("Pause did not stop playback");
            animation.Resume();if(!animation.IsPlaying)throw new System.Exception("Resume did not start playback");
            animation.Stop();if(animation.IsPlaying || animation.IsFinished)throw new System.Exception("Stop did not reset playback");
            animation.Play(Clip);Passed=true;
        }
        void OnUpdate(float timestep) {Finished=GetComponent<Hazel.SpriteAnimationComponent>().IsFinished;}
    }
}

namespace Migration {
    public class HierarchyProbe : Hazel.Entity {
        public Hazel.Entity ExpectedParent;
        public Hazel.Vector3 WorldBefore, LocalBefore, MovedWorld;
        public bool ParentMatched, MatrixMatched, QueuedGraphRetained, Detached;
        public int RequestStatus = -1, Updates;
        private Hazel.ParentingRequest request;
        void OnCreate() {
            WorldBefore = Translation; LocalBefore = LocalTranslation;
            ParentMatched = Parent != null && ExpectedParent != null && Parent.ID == ExpectedParent.ID;
            MatrixMatched = WorldMatrix.Position.X == Translation.X && WorldMatrix.Position.Y == Translation.Y;
        }
        void OnUpdate(float dt) {
            if (Updates++ == 0) {
                Translation = new Hazel.Vector3(WorldBefore.X + 1, WorldBefore.Y, WorldBefore.Z);
                MovedWorld = Translation;
                request = Detach(Hazel.ParentingMode.KeepWorld);
                QueuedGraphRetained = Parent != null && request.Status == Hazel.ParentingStatus.Pending;
            } else { Detached = Parent == null; RequestStatus = (int)request.Status; }
        }
    }
}

namespace Migration {
    public class UIProbe : Hazel.Entity {
        public bool Passed;
        void OnCreate() {
            if(!FindEntityByName("UI API canvas").HasComponent<Hazel.UICanvasComponent>() || !HasComponent<Hazel.UIRectComponent>() ||
               !HasComponent<Hazel.UILayoutComponent>() || !HasComponent<Hazel.UITextStyleComponent>() ||
               !HasComponent<Hazel.UINineSliceComponent>() || !HasComponent<Hazel.UIButtonComponent>())
                throw new System.Exception("Runtime UI component reflection binding missing");
            var canvas=FindEntityByName("UI API canvas").GetComponent<Hazel.UICanvasComponent>();canvas.Visible=false;canvas.Visible=true;canvas.Enabled=false;canvas.Enabled=true;canvas.Modal=true;
            var rect=GetComponent<Hazel.UIRectComponent>();rect.Visible=false;
            if(rect.Visible||!canvas.Visible)throw new System.Exception("Rect visibility aliases Canvas");
            rect.Visible=true;rect.Enabled=false;
            if(rect.Enabled||!canvas.Enabled)throw new System.Exception("Rect enabled state aliases Canvas");
            rect.Enabled=true;canvas.Visible=false;
            if(canvas.Visible||!rect.Visible)throw new System.Exception("Canvas visibility aliases Rect");
            canvas.Visible=true;
            var button=GetComponent<Hazel.UIButtonComponent>();button.Interactable=false;button.Interactable=true;
            string action;Hazel.Entity entity;
            if(button.TryConsumeAction(out action)||Hazel.GameUI.TryConsumeAction(out entity,out action))throw new System.Exception("Fresh scene retained UI actions");
            if(Hazel.GameUI.IsPointerCaptured)throw new System.Exception("Fresh scene retained pointer capture");
            var text=GetComponent<Hazel.TextComponent>();text.Text="Managed UI";
            foreach(var invalid in new[]{"A\0B","\ud800","\udfff"}) {
                bool rejected=false;try{text.Text=invalid;}catch(System.ArgumentException){rejected=true;}
                if(!rejected||text.Text!="Managed UI")throw new System.Exception("Managed text accepted invalid UTF-16/NUL or changed last valid text");
            }
            text.Text="\ufeffManaged UI";
            if(text.Text[0]!='\ufeff')throw new System.Exception("Managed BOM text was not accepted/preserved");
            text.Text="Managed UI";
            if(rect.LayoutVersion!=3)throw new System.Exception("Canonical UI schema mismatch");
            var measured=FindEntityByName("Measured API target").GetComponent<Hazel.UIRectComponent>();
            measured.Width=new Hazel.UISizing(Hazel.UISizeMode.Fixed,137,20,150);
            var sizing=measured.Width;
            if(measured.LayoutVersion!=3||sizing.Mode!=Hazel.UISizeMode.Fixed||sizing.FixedSize!=137||sizing.Minimum!=20||sizing.Maximum!=150)throw new System.Exception("Measured sizing API roundtrip failed");
            bool invalidSizing=false;try{measured.Width=new Hazel.UISizing(Hazel.UISizeMode.Fixed,float.NaN,20,10);}catch(System.InvalidOperationException){invalidSizing=true;}
            if(!invalidSizing||measured.Width.FixedSize!=137)throw new System.Exception("Invalid sizing mutation changed accepted runtime state");
            var container=GetComponent<Hazel.UILayoutComponent>();
            container.SetGrid(new[]{new Hazel.UITrack(Hazel.UITrackMode.Fixed,120),new Hazel.UITrack(Hazel.UITrackMode.Fill,2,10,200)},new[]{new Hazel.UITrack(Hazel.UITrackMode.Fixed,80)},new Hazel.Vector2(4,6));
            measured.Cell=new Hazel.UIGridCell(0,1);
            if(container.Settings.Kind!=Hazel.UIContainerKind.Grid||container.Columns.Length!=2||container.Columns[1].Limit!=200||measured.Cell.Column!=1)throw new System.Exception("Managed grid ABI roundtrip failed");
            bool invalidGrid=false;try{container.SetGrid(new Hazel.UITrack[0],container.Rows,new Hazel.Vector2(0,0));}catch(System.InvalidOperationException){invalidGrid=true;}
            if(!invalidGrid||container.Columns.Length!=2)throw new System.Exception("Invalid grid mutation changed accepted state");
            Passed=canvas.Visible&&canvas.Enabled&&canvas.Modal&&rect.Visible&&rect.Enabled&&button.Interactable;
        }
    }
}
