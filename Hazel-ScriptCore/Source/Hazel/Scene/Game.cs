namespace Hazel {
    // Live Play/player settings; persistence belongs to project-scoped SaveData.
    public static class Game {
        public static float AudioVolume {get=>InternalCalls.Runtime_AudioVolume(0,false);set=>InternalCalls.Runtime_AudioVolume(value,true);}
        // Deferred until the host returns from callbacks. Hazelnut stops Play.
        public static void Exit()=>InternalCalls.Runtime_Exit();
    }
}
