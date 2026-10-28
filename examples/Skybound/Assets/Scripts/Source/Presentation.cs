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

        }
        void OnUpdate(float dt) { Fit(); }
    }
    public class MenuOwner : Entity {
        void OnUpdate(float dt) {
            UIEvent e;
            while(GameUI.TryConsumeEvent(out e)) {
                if(e.Kind==UIEventKind.Activated && e.Action=="play") Scene.LoadScene("Scenes/Flight.hazel");
                else if(e.Kind==UIEventKind.Cancel || (e.Kind==UIEventKind.Activated && e.Action=="menu")) Scene.LoadScene("Scenes/MainMenu.hazel");
            }
        }
    }
}
