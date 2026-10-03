using System;
using Hazel;
namespace Skybound {
    public class FitCamera : Entity {
        public float MinimumHeight = 12;
        public float MinimumWidth = 16;
        private CameraComponent camera;
        void OnCreate() { camera=GetComponent<CameraComponent>();Fit(); }
        void Fit() {
            camera.OrthographicSize=Math.Max(Math.Max(1,MinimumHeight),Math.Max(1,MinimumWidth)/camera.AspectRatio);
            // Fixed playable floor at -4; trim pipes at that boundary and fill down to the actual viewport bottom.
            var ground=FindEntityByName("Ground");var fill=FindEntityByName("GroundFill");
            if(ground!=null && fill!=null) {
                float width=camera.OrthographicSize*camera.AspectRatio+2;
                ground.Scale=new Vector3(width,.9f,1);ground.Translation=new Vector3(0,-4.45f,.2f);
                float bottom=-camera.OrthographicSize/2-1;float height=Math.Max(.1f,-4.9f-bottom);
                fill.Scale=new Vector3(width,height,1);fill.Translation=new Vector3(0,-4.9f-height/2,.15f);
            }
        }
        void OnUpdate(float dt) { Fit(); }
    }
    public class SceneButton : Entity {
        public int Destination; // 0: flight, 1: menu
        public Vector2 HalfSize = new Vector2(2,0.6f);
        private bool mouseHeld, enterHeld;
        void OnCreate() { mouseHeld=Input.IsMouseButtonDown(MouseCode.Left);enterHeld=Input.IsKeyDown(KeyCode.Enter); }
        void OnUpdate(float dt) {
            bool mouse=Input.IsMouseButtonDown(MouseCode.Left),enter=Input.IsKeyDown(KeyCode.Enter);
            Vector2 p;var center=Translation;
            bool clicked=mouse&&!mouseHeld&&Input.GetMouseWorldPosition(out p)&&Math.Abs(p.X-center.X)<HalfSize.X&&Math.Abs(p.Y-center.Y)<HalfSize.Y;
            if(clicked||(Destination==0&&enter&&!enterHeld)) Scene.LoadScene(Destination==1?"Scenes/MainMenu.hazel":"Scenes/Flight.hazel");
            mouseHeld=mouse;enterHeld=enter;
        }
    }
}
