using Hazel;

namespace RenderingReference {
    public class LocalLightMotion : Entity {
        private Vector3 center;
        private float time;
        void OnCreate() { center = Translation; }
        void OnUpdate(float dt) {
            time += dt;
            Translation = center + new Vector3((float)System.Math.Sin(time * .7f) * 1.2f,
                (float)System.Math.Sin(time * .43f) * .5f, 0);
        }
    }
}
