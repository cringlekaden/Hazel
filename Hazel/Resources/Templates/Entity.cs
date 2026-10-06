using Hazel;

namespace @NAMESPACE@ {
    public class @CLASS@ : Entity {
        public Prefab SpawnAsset;
        private Entity spawned;
        void OnCreate() { }
        void OnUpdate(float dt) {
            // Select SpawnAsset in the Inspector after Build Scripts.
            if (Input.IsKeyDown(KeyCode.Space) && spawned == null && SpawnAsset != null && SpawnAsset.IsAssigned)
                spawned = Entity.Instantiate(SpawnAsset, Translation);
            if (Input.IsKeyDown(KeyCode.R) && spawned != null) { spawned.Destroy(); spawned = null; }
        }
        void OnDestroy() { if (spawned != null) spawned.Destroy(); }
    }
}
