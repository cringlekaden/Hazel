using System;

namespace Hazel
{
    public static class Scene
    {
        /// <summary>
        /// Request an asset-relative .hazel scene from a main-thread callback for the next runtime update.
        /// This does not synchronously load a scene or report loading success.
        /// First request wins until the boundary; Stop cancels pending requests.
        /// Failure is logged and the current scene continues running.
        /// </summary>
        public static void LoadScene(string assetPath)
        {
            if (string.IsNullOrWhiteSpace(assetPath))
                throw new ArgumentException("Supply an asset-relative .hazel path", nameof(assetPath));
            InternalCalls.Scene_LoadScene(assetPath);
        }
    }
}
