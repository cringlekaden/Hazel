// Exercises the actual published Hazel managed API in native embedding tests.
using System;
namespace Migration {
    public class Probe : Hazel.Entity {
        public float Speed = 2.5f;
        public char Character = '\u00e9';
        public ulong Unsigned = 0xfedcba9876543210UL;
        public Hazel.Vector3 Position = new Hazel.Vector3(1, 2, 3);
        public float Evaluate(float timestep) {
            Position = Position + new Hazel.Vector3(Speed * timestep);
            return Position.X + Position.Y + Position.Z;
        }
        public static void ThrowManaged() { throw new InvalidOperationException("Managed fixture exception"); }
    }
}
