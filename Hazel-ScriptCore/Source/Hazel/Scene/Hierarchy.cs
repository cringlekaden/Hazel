using System;
using System.Runtime.InteropServices;
namespace Hazel {
    public enum ParentingMode { KeepWorld = 0, KeepLocal = 1 }
    public enum ParentingStatus { Pending = 0, Applied = 1, Rejected = 2, Expired = 3 }
    // Snapshot of all four world-matrix columns; preserves shear instead of inventing TRS values.
    [StructLayout(LayoutKind.Sequential)]
    public struct Matrix4 {
        public Vector4 Column0, Column1, Column2, Column3;
        public Vector3 Position => new Vector3(Column3.X, Column3.Y, Column3.Z);
    }
    public sealed class ParentingRequest {
        private readonly ulong Scene, Request;
        internal ParentingRequest(ulong scene, ulong request) { Scene = scene; Request = request; }
        public ParentingStatus Status => (ParentingStatus)InternalCalls.Hierarchy_GetStatus(Scene, Request);
        public string Reason => InternalCalls.Hierarchy_GetReason(Scene, Request);
    }
}
