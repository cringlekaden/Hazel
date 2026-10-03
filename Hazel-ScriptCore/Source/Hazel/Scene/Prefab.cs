using System;
namespace Hazel {
    // Authored asset reference. Select a project prefab in Hazelnut's script Inspector.
    public sealed class Prefab {
        public readonly string Path;
        public bool IsAssigned { get { return !String.IsNullOrEmpty(Path); } }
        public Prefab(string path) { Path = path ?? ""; }
    }
}
