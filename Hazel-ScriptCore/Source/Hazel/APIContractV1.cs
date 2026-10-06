namespace Hazel {
    // Public contract marker used by native staging and SDK/package validation.
    // Patch engine commits may share this ABI; increment only for incompatible changes.
    public static class APIContractV1 { public const int Version = 1; }
}
