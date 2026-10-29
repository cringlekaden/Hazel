using System;
using System.Runtime.CompilerServices;

namespace Hazel
{
    public static class MacOSFeasibility
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int NativeAdd(int left, int right);

        public static int Exercise(int seed)
        {
            // Exercise ordinary profile assemblies as well as Hazel's internal
            // call registration convention.
            if (String.Concat("domain-", seed.ToString()).Length < 8)
                throw new InvalidOperationException("Managed profile failure");
            return NativeAdd(seed, 5);
        }
    }
}
