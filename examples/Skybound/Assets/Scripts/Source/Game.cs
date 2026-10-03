using System;
using Hazel;
namespace Skybound {
    public class Game : Entity {
        public float ScrollSpeed=2.8f;
        public float Gravity=9.6f;
        public float FlapSpeed=5;
        public float GapHalf=1.7f;
        private Flight flight;
        private Entity bird,ready;
        private readonly Entity[] upper=new Entity[4],lower=new Entity[4];
        private Entity[] overlay;
        private TextComponent score,result;
        private bool mouseHeld,spaceHeld,restartHeld,escapeHeld;
        private bool shown;
        private int previousScore;
        void OnCreate() {
            flight=new Flight(ScrollSpeed,Gravity,FlapSpeed,GapHalf);
            bird=FindEntityByName("Wisp");ready=FindEntityByName("Ready");
            score=FindEntityByName("Score").GetComponent<TextComponent>();
            result=FindEntityByName("Result").GetComponent<TextComponent>();
            for(int i=0;i<4;i++) { upper[i]=FindEntityByName("Upper"+i);lower[i]=FindEntityByName("Lower"+i); }
            string[] names={"GameOverPanel","Result","ResultHint","Restart","RestartLabel","Menu","MenuLabel"};
            // Panel and captions are editor-authored. Move the entire overlay at death.
            overlay=new Entity[names.Length];
            for(int i=0;i<names.Length;i++)overlay[i]=FindEntityByName(names[i]);
            mouseHeld=Input.IsMouseButtonDown(MouseCode.Left);spaceHeld=Input.IsKeyDown(KeyCode.Space);
            restartHeld=Input.IsKeyDown(KeyCode.R);escapeHeld=Input.IsKeyDown(KeyCode.Escape);
            Sync();
        }
        private bool Hit(float x,float y) {
            Vector2 p;return Input.GetMouseWorldPosition(out p)&&Math.Abs(p.X-x)<2&&Math.Abs(p.Y-y)<.6f;
        }
        void Sync() {
            bird.Translation=new Vector3(Flight.BirdX,flight.Y,.3f);
            for(int i=0;i<4;i++) {
                upper[i].Translation=new Vector3(flight.Gates[i].X,flight.Gates[i].Gap+flight.GapHalf+4.5f,0);
                lower[i].Translation=new Vector3(flight.Gates[i].X,flight.Gates[i].Gap-flight.GapHalf-4.5f,0);
            }
        }
        void OnUpdate(float dt) {
            bool mouse=Input.IsMouseButtonDown(MouseCode.Left),space=Input.IsKeyDown(KeyCode.Space),restart=Input.IsKeyDown(KeyCode.R),escape=Input.IsKeyDown(KeyCode.Escape);
            bool click=mouse&&!mouseHeld;
            if(escape&&!escapeHeld) Scene.LoadScene("Scenes/MainMenu.hazel");
            if(flight.Phase==Flight.State.Dead) {
                // Restart only on a new event after death. A fatal flap never restarts.
                if((restart&&!restartHeld)||(space&&!spaceHeld)||(click&&Hit(0,-1))) Scene.LoadScene("Scenes/Flight.hazel");
                else if(click&&Hit(0,-2.5f)) Scene.LoadScene("Scenes/MainMenu.hazel");
            } else {
                var before=flight.Phase;
                Vector2 mouseWorld;
                bool flap=(space&&!spaceHeld)||(click&&Input.GetMouseWorldPosition(out mouseWorld));
                flight.Advance(dt,flap);
                if(before==Flight.State.Ready&&flight.Phase!=before) {
                    ready.Translation=new Vector3(0,40,1);
                    Console.WriteLine("Skybound: flying");
                }
                Sync();
                score.Text="SCORE  "+flight.Score;
                if(flight.Score!=previousScore) { previousScore=flight.Score;Console.WriteLine("Skybound: score "+flight.Score); }
                if(flight.Phase==Flight.State.Dead&&!shown) {
                    shown=true;result.Text="FLIGHT ENDED  /  "+flight.Score;
                    foreach(var item in overlay)item.Translation=item.Translation+new Vector3(0,-40,0);
                    Console.WriteLine("Skybound: game over");
                }
            }
            mouseHeld=mouse;spaceHeld=space;restartHeld=restart;escapeHeld=escape;
        }
    }
    public class CloudDrift : Entity {
        public float Speed=.15f;
        void OnUpdate(float dt) {
            var p=Translation;p.X-=Speed*Math.Min(dt,.25f);if(p.X<-15)p.X=15;Translation=p;
        }
    }
}
