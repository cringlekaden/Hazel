namespace Hazel
{
	public enum MouseCode { Left = 0, Right = 1, Middle = 2 }

    public static class Input
	{
        public static bool IsMouseButtonDown(MouseCode button) { return InternalCalls.Input_IsMouseButtonDown(button); }
        // Ray intersection with world z=0, valid only inside the active runtime viewport.
        public static bool GetMouseWorldPosition(out Vector2 position) { return InternalCalls.Input_GetMouseWorldPosition(out position); }

		public static bool IsKeyDown(KeyCode keycode)
		{
			return InternalCalls.Input_IsKeyDown(keycode);
		}
	}
}
