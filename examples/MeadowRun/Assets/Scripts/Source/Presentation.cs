using System;
using Hazel;
namespace MeadowRun {
    public class FitCamera : Entity {
        public float MinimumHeight = 12;
        public float MinimumWidth = 16;
        private CameraComponent camera;
        void OnCreate() { camera=GetComponent<CameraComponent>();Fit(); }
        void Fit() { camera.OrthographicSize=Math.Max(Math.Max(1,MinimumHeight),Math.Max(1,MinimumWidth)/camera.AspectRatio); }
        void OnUpdate(float dt) { Fit(); }
    }
    public class SceneButton : Entity {
        public int Destination; // 0: meadow, 1: menu
        public Vector2 HalfSize = new Vector2(2,0.6f);
        private bool mouseHeld, enterHeld;
        void OnCreate() { mouseHeld=Input.IsMouseButtonDown(MouseCode.Left);enterHeld=Input.IsKeyDown(KeyCode.Enter); }
        void OnUpdate(float dt) {
            bool mouse=Input.IsMouseButtonDown(MouseCode.Left),enter=Input.IsKeyDown(KeyCode.Enter);
            Vector2 p;var center=Translation;
            bool clicked=mouse&&!mouseHeld&&Input.GetMouseWorldPosition(out p)&&Math.Abs(p.X-center.X)<HalfSize.X&&Math.Abs(p.Y-center.Y)<HalfSize.Y;
            if(clicked||(Destination==0&&enter&&!enterHeld)) Scene.LoadScene(Destination==1?"Scenes/MainMenu.hazel":"Scenes/Meadow.hazel");
            mouseHeld=mouse;enterHeld=enter;
        }
    }
}
