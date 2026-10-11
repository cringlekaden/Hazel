using System;
using Hazel;
namespace LastLightkeeper {
    public sealed class Controls {
        private bool e,enter,space,escape,q,n,m,c;
        public bool Interact,Advance,Escape,Journal,New,Menu,Credits;
        public Controls(){Poll();}
        public void Poll(){
            bool a=Input.IsKeyDown(KeyCode.E),b=Input.IsKeyDown(KeyCode.Enter),d=Input.IsKeyDown(KeyCode.Space),f=Input.IsKeyDown(KeyCode.Escape),g=Input.IsKeyDown(KeyCode.Q),h=Input.IsKeyDown(KeyCode.N),i=Input.IsKeyDown(KeyCode.M),j=Input.IsKeyDown(KeyCode.C);
            Interact=a&&!e;Advance=(b&&!enter)||(d&&!space);Escape=f&&!escape;Journal=g&&!q;New=h&&!n;Menu=i&&!m;Credits=j&&!c;
            e=a;enter=b;space=d;escape=f;q=g;n=h;m=i;c=j;
        }
    }
    public class View : Entity {
        public float Height=18, Width=29;
        public bool Follow=false;
        public Entity Target;
        private CameraComponent camera;
        void OnCreate(){camera=GetComponent<CameraComponent>();Fit();}
        void Fit(){camera.OrthographicSize=Math.Max(Height,Width/Math.Max(.1f,camera.AspectRatio));}
        void OnUpdate(float dt){
            Fit();if(!Follow || Target==null || !Target.IsValid)return;
            var p=Target.Translation;var t=Translation;
            // HUD space favors the path ahead. Smooth without framerate-dependent lerp.
            float weight=1-(float)Math.Exp(-7*Math.Max(0,dt));
            t.X+=(Math.Max(-12,Math.Min(12,p.X))-t.X)*weight;
            t.Y+=(Math.Max(-8,Math.Min(10,p.Y+1))-t.Y)*weight;
            Translation=t;
        }
    }
    public class Menu : Entity {
        public Entity Heading,Subtitle,Status,StartButton,StartLabel,NewButton,CreditsButton;
        public bool Ending=false;
        private TextComponent status;
        private Controls keys;
        private bool held,confirm,canContinue,damaged;
        private float confirmationTime;
        private Journey journey;
        void OnCreate(){
            keys=new Controls();held=Input.IsMouseButtonDown(MouseCode.Left);status=Status.GetComponent<TextComponent>();
            try {var raw=SaveData.Read("journey");journey=Journey.Decode(raw);canContinue=!String.IsNullOrEmpty(raw);}
            catch(Exception ex){journey=new Journey();damaged=true;status.Text="Journey could not be read. Original kept.\n"+ex.Message;Console.WriteLine("Lightkeeper save: "+ex.Message);}
            if(Ending){
                Heading.GetComponent<TextComponent>().Text=journey.Has(Restoration.Watch)?"THE DRY\nWAY HOME":"THE LOWER\nBEACON";
                Subtitle.GetComponent<TextComponent>().Text=journey.Has(Restoration.Watch)?"Mara is safe. Orrin has reached the survey ledge.\nThe observatory still has people inside.":"Orrin can get through the reef.\nMara's lamp is moving beneath the cistern.";
                status.Text=journey.Has(Restoration.Watch)?"Chapter two complete. Progress saved.\nThe observatory and upper tower are the next chapter.":"The cliff path is open.";
            } else if(!damaged)status.Text=canContinue?Story.Objective(journey):"Three nights without a beacon.\nBegin at Orrin's landing.";
            StartLabel.GetComponent<TextComponent>().Text=Ending && journey.Has(Restoration.Watch)?"Enter   Title":canContinue?"Enter   Continue":"Enter   Begin";
        }
        bool Click(Entity button,bool edge){Vector2 p;if(!edge || button==null || !Input.GetMouseWorldPosition(out p))return false;var t=button.Translation;return Math.Abs(p.X-t.X)<4.4 && Math.Abs(p.Y-t.Y)<.65;}
        void Start(){
            if(damaged){status.Text="The saved journey is unreadable. It has been kept.\nPress N twice to replace it with a new game.";return;}
            if(!canContinue){journey=new Journey();if(!Persist())return;}
            Scene.LoadScene(Ending && journey.Has(Restoration.Watch)?"Scenes/MainMenu.hazel":journey.ResumeScene);
        }
        bool Persist(){try{SaveData.Write("journey",journey.Encode());canContinue=true;damaged=false;return true;}catch(Exception ex){status.Text="Cannot save: "+ex.Message;Console.WriteLine("Lightkeeper save: "+ex.Message);return false;}}
        void OnUpdate(float dt){
            keys.Poll();bool mouse=Input.IsMouseButtonDown(MouseCode.Left),edge=mouse&&!held;held=mouse;
            if(confirmationTime>0){confirmationTime-=dt;if(confirmationTime<=0){confirm=false;status.Text="New game cancelled. Save kept.";}}
            if(keys.Credits || Click(CreditsButton,edge)){status.Text="The Last Lightkeeper\nArt and sound by Kenney : CC0. Engine: Hazel.\nThis independent game is not endorsed by Kenney.";}
            else if(keys.New || Click(NewButton,edge)){
                if((canContinue || damaged) && !confirm){confirm=true;confirmationTime=6;status.Text="Start again? This replaces the saved journey.\nPress N again within six seconds to confirm.";}
                else {journey=new Journey();if(Persist())Scene.LoadScene("Scenes/Breakwater.hazel");}
            }
            else if(keys.Advance || Click(StartButton,edge))Start();
        }
    }
}
