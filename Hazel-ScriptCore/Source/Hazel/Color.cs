using System;
namespace Hazel {
    // Component, text and style APIs store linear RGB; alpha is coverage.
    public static class Color {
        public static float FromSRGB(float value) => value <= 0.04045f ? value / 12.92f : (float)Math.Pow((value + 0.055f) / 1.055f, 2.4);
        public static float ToSRGB(float value) => value <= 0.0031308f ? value * 12.92f : 1.055f * (float)Math.Pow(value, 1.0 / 2.4) - 0.055f;
        public static Vector4 FromSRGB(Vector4 value) => new Vector4(FromSRGB(value.X), FromSRGB(value.Y), FromSRGB(value.Z), value.W);
        public static Vector4 ToSRGB(Vector4 value) => new Vector4(ToSRGB(value.X), ToSRGB(value.Y), ToSRGB(value.Z), value.W);
    }
}
