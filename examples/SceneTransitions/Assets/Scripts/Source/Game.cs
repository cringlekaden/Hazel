using Hazel;

namespace SceneTransitions
{
    public class PlayButton : Entity {
        void OnUpdate(float dt) { string action;if(GetComponent<UIButtonComponent>().TryConsumeAction(out action))Scene.LoadScene("Scenes/Level1.hazel"); }
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
    public class MenuButton : Entity {
        void OnUpdate(float dt) { string action;if(GetComponent<UIButtonComponent>().TryConsumeAction(out action))Scene.LoadScene("Scenes/MainMenu.hazel"); }
    }
}
