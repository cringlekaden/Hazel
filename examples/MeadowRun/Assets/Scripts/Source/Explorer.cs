using System;
using Hazel;
namespace MeadowRun {
    public class Explorer : Entity {
        public float Speed = 3.4f;
        private Rigidbody2DComponent body;
        void OnCreate() { body = GetComponent<Rigidbody2DComponent>(); }
        void OnUpdate(float dt) {
            float x=(Input.IsKeyDown(KeyCode.D)||Input.IsKeyDown(KeyCode.Right)?1:0)-(Input.IsKeyDown(KeyCode.A)||Input.IsKeyDown(KeyCode.Left)?1:0);
            float y=(Input.IsKeyDown(KeyCode.W)||Input.IsKeyDown(KeyCode.Up)?1:0)-(Input.IsKeyDown(KeyCode.S)||Input.IsKeyDown(KeyCode.Down)?1:0);
            var direction=new Vector2(x,y);
            float length=direction.Length();
            body.LinearVelocity=length>0 ? direction*(Math.Max(0,Speed)/length) : Vector2.Zero;
        }
    }
    // Progress belongs to this scene instance. Collectibles remain authored entities.
    public class Meadow : Entity {
        public float PickupRadius = 0.65f;
        public float PondRadius = 1.05f;
        private Entity player, pond, exit;
        private readonly Entity[] seeds=new Entity[5];
        private readonly bool[] collected=new bool[5];
        private TextComponent progress, hint;
        private int count;
        private float messageTime;
        private bool restartHeld, escapeHeld;
        void OnCreate() {
            player=FindEntityByName("Explorer");pond=FindEntityByName("Pond");exit=FindEntityByName("Exit");
            progress=FindEntityByName("Progress").GetComponent<TextComponent>();
            hint=FindEntityByName("Hint").GetComponent<TextComponent>();
            for(int i=0;i<5;i++) seeds[i]=FindEntityByName("Seed"+i);
            restartHeld=Input.IsKeyDown(KeyCode.R);escapeHeld=Input.IsKeyDown(KeyCode.Escape);
            progress.Text="Lantern seeds  0 / 5";
        }
        static bool Near(Entity a,Entity b,float radius) {
            var p=a.Translation;var q=b.Translation;
            return (p.X-q.X)*(p.X-q.X)+(p.Y-q.Y)*(p.Y-q.Y)<radius*radius;
        }
        void OnUpdate(float dt) {
            bool restart=Input.IsKeyDown(KeyCode.R),escape=Input.IsKeyDown(KeyCode.Escape);
            if(escape&&!escapeHeld) Scene.LoadScene("Scenes/MainMenu.hazel");
            else if(restart&&!restartHeld) Scene.LoadScene("Scenes/Meadow.hazel");
            restartHeld=restart;escapeHeld=escape;
            for(int i=0;i<5;i++) if(!collected[i]&&Near(player,seeds[i],Math.Max(0.1f,PickupRadius))) {
                collected[i]=true;count++;
                seeds[i].Translation=new Vector3(500+i,0,0);
                progress.Text="Lantern seeds  "+count+" / 5";
                Console.WriteLine("MeadowRun: seed "+count);
                if(count==5) {
                    hint.Text="All five found! Follow the north-east trail.";
                    FindEntityByName("ExitLabel").GetComponent<TextComponent>().Text="TRAIL OPEN";
                }
            }
            if(Near(player,pond,Math.Max(0.1f,PondRadius))) {
                player.Translation=new Vector3(-6,-3,0.3f);
                player.GetComponent<Rigidbody2DComponent>().LinearVelocity=Vector2.Zero;
                messageTime=2.5f;hint.Text="Splash! Back at camp. Your seeds are safe.";
                Console.WriteLine("MeadowRun: checkpoint");
            }
            if(messageTime>0) {
                messageTime-=dt;
                if(messageTime<=0) hint.Text=count==5?"All five found! Follow the north-east trail.":"Find five lantern seeds. Stay out of the pond.";
            }
            if(count==5&&Near(player,exit,0.8f)) Scene.LoadScene("Scenes/Complete.hazel");
        }
    }
}
