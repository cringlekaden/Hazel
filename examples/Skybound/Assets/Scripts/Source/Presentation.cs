using System;
using Hazel;
namespace Skybound {
    public class FitCamera : Entity {
        public float MinimumHeight = 12;
        public float MinimumWidth = 16;
        private CameraComponent camera;
        void OnCreate() { camera=GetComponent<CameraComponent>();Fit(); }
        public static float Floor(CameraComponent view) { return -view.OrthographicSize/2+.9f; }
        public static float Ceiling(CameraComponent view) { return view.OrthographicSize/2-1.8f; }
        public void Fit() {
            camera.OrthographicSize=Math.Max(Math.Max(1,MinimumHeight),Math.Max(1,MinimumWidth)/camera.AspectRatio);
            // Ground's bottom edge is the camera bottom; gameplay uses its top as the collision floor.
            var ground=FindEntityByName("Ground");
            if(ground!=null) {
                float width=camera.OrthographicSize*camera.AspectRatio+2;
                ground.Scale=new Vector3(width,.9f,1);ground.Translation=new Vector3(0,Floor(camera)-.45f,.2f);
            }
            foreach(var name in new[]{"HUD ribbon","Score","HUD controls"}) {
                var item=FindEntityByName(name);
                if(item!=null) { var p=item.Translation;p.Y=camera.OrthographicSize/2-(name=="Score"?1.2f:name=="HUD controls"?1.15f:1);item.Translation=p; }
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
