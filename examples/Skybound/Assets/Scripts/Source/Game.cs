using System;
using Hazel;
namespace Skybound {
    public class Game : Entity {
        public Prefab UpperPipe;
        public Prefab LowerPipe;
        public float ScrollSpeed=2.8f;
        public float Gravity=9.6f;
        public float FlapSpeed=5;
        public float GapHalf=1.7f;
        private Flight flight;
        private Entity bird,ready,restartButton,menuButton;
        private readonly Entity[] upper=new Entity[4],lower=new Entity[4];
        private readonly int[] generations=new int[4];
        private Entity[] overlay;
        private TextComponent score,result;
        private bool mouseHeld,spaceHeld,restartHeld,escapeHeld;
        private bool shown;
        private int previousScore;
        void OnCreate() {

            bird=FindEntityByName("Wisp");ready=FindEntityByName("Ready");
            score=FindEntityByName("Score").GetComponent<TextComponent>();
            result=FindEntityByName("Result").GetComponent<TextComponent>();
            flight=new Flight(ScrollSpeed,Gravity,FlapSpeed,GapHalf);
            for(int i=0;i<4;i++) SpawnPair(i);
            restartButton=FindEntityByName("Restart");menuButton=FindEntityByName("Menu");
            string[] names={"GameOverPanel","Result","ResultHint","Restart","RestartLabel","Menu","MenuLabel"};
            // Panel and captions are editor-authored. Move the entire overlay at death.
            overlay=new Entity[names.Length];
            for(int i=0;i<names.Length;i++)overlay[i]=FindEntityByName(names[i]);
            mouseHeld=Input.IsMouseButtonDown(MouseCode.Left);spaceHeld=Input.IsKeyDown(KeyCode.Space);
            restartHeld=Input.IsKeyDown(KeyCode.R);escapeHeld=Input.IsKeyDown(KeyCode.Escape);
            Sync();
        }
        void OnDestroy() {
            foreach(var pipe in upper) if(pipe!=null) pipe.Destroy();
            foreach(var pipe in lower) if(pipe!=null) pipe.Destroy();
        }
        void SpawnPair(int i) {
            var gate=flight.Gates[i];float bottom=gate.Gap-flight.GapHalf;
            float lowHeight=Math.Max(.1f,bottom+4);
            float highHeight=20;
            upper[i]=Entity.Instantiate(UpperPipe,new Vector3(gate.X,gate.Gap+flight.GapHalf+highHeight/2,0),Vector3.Zero,new Vector3(1.1f,highHeight,1));
            lower[i]=Entity.Instantiate(LowerPipe,new Vector3(gate.X,bottom-lowHeight/2,0),Vector3.Zero,new Vector3(1.1f,lowHeight,1));
            generations[i]=gate.Generation;
        }
        private bool Hit(Entity button) {
            var center=button.Translation;Vector2 p;return Input.GetMouseWorldPosition(out p)&&Math.Abs(p.X-center.X)<2&&Math.Abs(p.Y-center.Y)<.6f;
        }
        void Sync() {
            bird.Translation=new Vector3(Flight.BirdX,flight.Y,.3f);
            for(int i=0;i<4;i++) {
                if(generations[i]!=flight.Gates[i].Generation) {
                    upper[i].Destroy();lower[i].Destroy();SpawnPair(i);
                }
                var top=upper[i].Translation;top.X=flight.Gates[i].X;upper[i].Translation=top;
                var bottom=lower[i].Translation;bottom.X=flight.Gates[i].X;lower[i].Translation=bottom;
            }
        }
        void OnUpdate(float dt) {
            bool mouse=Input.IsMouseButtonDown(MouseCode.Left),space=Input.IsKeyDown(KeyCode.Space),restart=Input.IsKeyDown(KeyCode.R),escape=Input.IsKeyDown(KeyCode.Escape);
            bool click=mouse&&!mouseHeld;
            if(escape&&!escapeHeld) Scene.LoadScene("Scenes/MainMenu.hazel");
            if(flight.Phase==Flight.State.Dead) {
                // Restart only on a new event after death. A fatal flap never restarts.
                if((restart&&!restartHeld)||(space&&!spaceHeld)||(click&&Hit(restartButton))) Scene.LoadScene("Scenes/Flight.hazel");
                else if(click&&Hit(menuButton)) Scene.LoadScene("Scenes/MainMenu.hazel");
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
