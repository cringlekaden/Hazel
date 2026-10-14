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
        private UIButtonComponent button;
        void OnCreate() { button=GetComponent<UIButtonComponent>(); }
        void OnUpdate(float dt) {
            string action;
            if(button.TryConsumeAction(out action)) Scene.LoadScene(Destination==1?"Scenes/MainMenu.hazel":"Scenes/Meadow.hazel");
        }
    }
}
