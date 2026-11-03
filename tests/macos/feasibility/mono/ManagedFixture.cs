using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Hazel
{
    public static class MacOSFeasibility
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int NativeAdd(int left, int right);

        [DllImport("MonoPosixHelper", EntryPoint="Mono_Posix_Syscall_get_at_fdcwd")]
        private static extern int CurrentDirectoryDescriptor();
        [DllImport("System.Native", EntryPoint="SystemNative_GetEUid")]
        private static extern uint EffectiveUser();

        public static int Exercise(int seed)
        {
            // Exercise Mono's runtime-loaded helpers, which otool cannot see
            // in the embedding executable's static dependency list.
            if (CurrentDirectoryDescriptor() >= 0 || EffectiveUser() == UInt32.MaxValue)
                throw new InvalidOperationException("Native helper invocation failed");
            // Exercise ordinary profile assemblies as well as Hazel's internal
            // call registration convention.
            if (String.Concat("domain-", seed.ToString()).Length < 8)
                throw new InvalidOperationException("Managed profile failure");
            return NativeAdd(seed, 5);
        }
    }
}
