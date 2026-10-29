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
    public class MenuOwner : Entity {
        void OnUpdate(float dt) {
            UIEvent e;
            while(GameUI.TryConsumeEvent(out e)) {
                if(e.Kind==UIEventKind.Activated && e.Action=="play") Scene.LoadScene("Scenes/Meadow.hazel");
                else if(e.Kind==UIEventKind.Cancel || (e.Kind==UIEventKind.Activated && e.Action=="menu")) Scene.LoadScene("Scenes/MainMenu.hazel");
            }
        }
    }
}
