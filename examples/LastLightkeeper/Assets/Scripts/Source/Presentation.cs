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

        private Journey journey;
        void OnCreate(){
            keys=new Controls();held=Input.IsMouseButtonDown(MouseCode.Left);status=Status.GetComponent<TextComponent>();
            try {var raw=SaveData.Read("journey");journey=Journey.Decode(raw);canContinue=!String.IsNullOrEmpty(raw);}
            catch(Exception ex){journey=new Journey();damaged=true;status.Text="Your save could not be read. It has been kept.\nN: start over    C: credits";Console.WriteLine("Lightkeeper save: "+ex.Message);}
            if(Ending){
                Heading.GetComponent<TextComponent>().Text=journey.Has(Restoration.Observatory)?"THE SURVEY\nCREW":journey.Has(Restoration.Watch)?"THE DRY\nWAY HOME":"THE LOWER\nBEACON";
                Subtitle.GetComponent<TextComponent>().Text=journey.Has(Restoration.Observatory)?"Cal can reach the landing.\nNessa is holding the tower winch brake.":journey.Has(Restoration.Watch)?"Mara is safe. Orrin has reached the survey ledge.\nTwo people remain at the observatory.":"Orrin can get through the reef.\nMara's lamp is moving beneath the cistern.";
                status.Text=journey.Has(Restoration.Observatory)?"Progress saved.\nThe upper-tower chapter is not available yet.":journey.Has(Restoration.Watch)?"Mara's ledger is dry.\nTake it up the north path.":"The cliff path is open.";
            } else if(!damaged)status.Text=canContinue?Story.Objective(journey):"Three nights without a beacon.";
            StartLabel.GetComponent<TextComponent>().Text=damaged?"Enter   Save unavailable":Ending && journey.Has(Restoration.Observatory)?"Enter   Title":Ending && journey.Has(Restoration.Watch)?"Enter   Observatory":canContinue?"Enter   Continue":"Enter   Begin";
        }
        bool Click(Entity button,bool edge){Vector2 p;if(!edge || button==null || !Input.GetMouseWorldPosition(out p))return false;var t=button.Translation;return Math.Abs(p.X-t.X)<4.4 && Math.Abs(p.Y-t.Y)<.65;}
        void Start(){
            if(damaged){status.Text="Your save cannot be opened. It has been kept.\nPress N twice to start over.";return;}
            if(!canContinue && !Persist(new Journey()))return;
            // Keep the current session alive if departure cannot be saved.
            if(Ending && !Persist(journey))return;
            Scene.LoadScene(Ending && journey.Has(Restoration.Observatory)?"Scenes/MainMenu.hazel":Ending && journey.Has(Restoration.Watch)?"Scenes/Observatory.hazel":journey.ResumeScene);
        }
        bool Persist(Journey candidate){try{SaveData.Write("journey",candidate.Encode());journey=candidate;canContinue=true;damaged=false;return true;}catch(Exception ex){status.Text="Could not save. Check available disk space\nand write access, then try again.";Console.WriteLine("Lightkeeper save: "+ex.Message);return false;}}
        void NewGame(){
            if((canContinue || damaged) && !confirm){confirm=true;status.Text="Start over and replace your save?\nPress N again. Esc cancels.";}
            else if(Persist(new Journey()))Scene.LoadScene("Scenes/Breakwater.hazel");
        }
        void CancelNewGame(){confirm=false;status.Text=damaged?"Your save has been kept.":Story.Objective(journey);}
        void OnUpdate(float dt){
            keys.Poll();bool mouse=Input.IsMouseButtonDown(MouseCode.Left),edge=mouse&&!held;held=mouse;
            if(keys.Credits || Click(CreditsButton,edge)){confirm=false;status.Text="Art and sound: Kenney (CC0). Engine: Hazel.\nAn independent game; no Kenney endorsement.";}
            else if(keys.New || Click(NewButton,edge))NewGame();
            else if(keys.Escape && confirm)CancelNewGame();
            else if(keys.Advance || Click(StartButton,edge)){confirm=false;Start();}
        }
    }
}
