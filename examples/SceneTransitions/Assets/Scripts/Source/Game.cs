using Hazel;

namespace SceneTransitions
{
    // A single rectangular world-space control, not an editor UI or UI framework.
    public class PlayButton : Entity
    {
        private bool previous;
        void OnUpdate(float timestep)
        {
            bool down = Input.IsMouseButtonDown(MouseCode.Left);
            Vector2 mouse;
            if ((down && !previous && Input.GetMouseWorldPosition(out mouse) &&
                 System.Math.Abs(mouse.X) < 2 && System.Math.Abs(mouse.Y) < 0.65f) || Input.IsKeyDown(KeyCode.Enter))
                Scene.LoadScene("Scenes/Level1.hazel");
            previous = down;
        }
    }
    public class Player : Entity
    {
        public float Speed = 6;
        private Rigidbody2DComponent body;
        private bool jumpHeld;
        void OnCreate() { body = GetComponent<Rigidbody2DComponent>(); }
        void OnUpdate(float timestep)
        {
            float horizontal = (Input.IsKeyDown(KeyCode.D) ? 1 : 0) - (Input.IsKeyDown(KeyCode.A) ? 1 : 0);
            body.ApplyLinearImpulse(new Vector2(horizontal * Speed * timestep, 0), true);
            bool jump = Input.IsKeyDown(KeyCode.Space);
            if (jump && !jumpHeld && System.Math.Abs(body.LinearVelocity.Y) < 0.15f)
                body.ApplyLinearImpulse(new Vector2(0, 3.5f), true);
            jumpHeld = jump;
            if (Input.IsKeyDown(KeyCode.Escape)) Scene.LoadScene("Scenes/MainMenu.hazel");
        }
    }
    public class MenuButton : Entity
    {
        // A held mouse from the retired scene cannot activate this new control.
        private bool previous;
        void OnCreate() { previous = Input.IsMouseButtonDown(MouseCode.Left); }
        void OnUpdate(float timestep)
        {
            bool down = Input.IsMouseButtonDown(MouseCode.Left);
            Vector2 mouse;
            if (down && !previous && Input.GetMouseWorldPosition(out mouse) && mouse.X > -3 && mouse.X < -0.5f && mouse.Y > 3 && mouse.Y < 4)
                Scene.LoadScene("Scenes/MainMenu.hazel");
            previous = down;
        }
    }
}
