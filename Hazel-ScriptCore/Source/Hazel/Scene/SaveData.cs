namespace Hazel {
    /// <summary>Project-owned UTF-8 save slots. Nutella persists atomically; editor Play uses an isolated memory overlay.
    /// Missing slots return empty text. I/O, corrupt data and future versions throw; game code owns payload version/migration.
    /// Saves are never written inside Assets. Calls require an active runtime session.</summary>
    public static class SaveData {
        public static bool IsPersistent => InternalCalls.SaveData_IsPersistent();
        public static string Read(string slot) => InternalCalls.SaveData_Read(slot);
        public static void Write(string slot, string payload) => InternalCalls.SaveData_Write(slot, payload);
    }
}
