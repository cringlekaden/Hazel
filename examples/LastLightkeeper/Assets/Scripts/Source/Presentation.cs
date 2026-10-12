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
        public bool Ending=false,CreditsPage=false;
        private TextComponent status;
        private Controls keys;
        private bool held,confirm,canContinue,damaged;

        private Journey journey;
        void OnCreate(){
            keys=new Controls();held=Input.IsMouseButtonDown(MouseCode.Left);status=Status.GetComponent<TextComponent>();
            try {var raw=SaveData.Read("journey");journey=Journey.Decode(raw);canContinue=!String.IsNullOrEmpty(raw);}
            catch(Exception ex){journey=new Journey();damaged=true;status.Text="Your save could not be read. It has been kept.\nN: start over    C: credits";Console.WriteLine("Lightkeeper save: "+ex.Message);}
            if(CreditsPage){
                Heading.GetComponent<TextComponent>().Text="THE LAST\nLIGHTKEEPER";
                Subtitle.GetComponent<TextComponent>().Text="Art and sound: Kenney (CC0).\nEngine: Hazel. Built with the Hazel community.";
                status.Text="All six asset packs and their licenses travel\nwith the game. Full notices: Assets/Licenses.\nAn independent game; no Kenney endorsement.";
                StartLabel.GetComponent<TextComponent>().Text="Enter   Title";
                NewButton.Scale=new Vector3(.001f,.001f,1);CreditsButton.Scale=new Vector3(.001f,.001f,1);
                FindEntityByName("NewLabel").GetComponent<TextComponent>().Text="";FindEntityByName("CreditsLabel").GetComponent<TextComponent>().Text="";
                FindEntityByName("Controls").GetComponent<TextComponent>().Text="Enter or Esc: return to title";return;
            }
            if(Ending){
                bool finished=journey.Has(Restoration.Home),survey=journey.Has(Restoration.Observatory);
                Heading.GetComponent<TextComponent>().Text=finished?"THE MORNING\nBOAT":survey?"THE SURVEY\nCREW":journey.Has(Restoration.Watch)?"THE DRY\nWAY HOME":"THE LOWER\nBEACON";
                Subtitle.GetComponent<TextComponent>().Text=finished?"The main light is steady. Everyone is aboard.\nTomorrow's crossing brings timber for the steps.":survey?"Cal can reach the landing.\nNessa is holding the tower winch brake.":journey.Has(Restoration.Watch)?"Mara is safe. Orrin has reached the survey ledge.\nTwo people remain at the observatory.":"Orrin can get through the reef.\nMara's lamp is moving beneath the cistern.";
                status.Text=finished?"Journey saved. Continue revisits the landing.\nC: credits":survey?"Cal's settings are in the ledger.\nTake the winch walk to the upper tower.":journey.Has(Restoration.Watch)?"Mara's ledger is dry.\nTake it up the north path.":"The cliff path is open.";
            } else if(!damaged)status.Text=canContinue?Story.Objective(journey):"Three nights without a beacon.";
            if(!Ending && !damaged){
                if(journey.Has(Restoration.Home))Subtitle.GetComponent<TextComponent>().Text="Everyone reached the morning boat.\nTomorrow's crossing brings timber for the steps.";
                else if(journey.Has(Restoration.TowerBeacon))Subtitle.GetComponent<TextComponent>().Text="Everyone is back at the landing.\nOrrin is ready for the morning crossing.";
                else if(journey.Has(Restoration.TowerDrive))Subtitle.GetComponent<TextComponent>().Text="The drive is steady.\nIona can reach the main lantern.";
                else if(journey.Has(Restoration.TowerEntry))Subtitle.GetComponent<TextComponent>().Text="Iona is at the upper tower.\nThe main drive still needs light.";
            }
            StartLabel.GetComponent<TextComponent>().Text=damaged?"Enter   Save unavailable":Ending && journey.Has(Restoration.Home)?"Enter   Title":Ending && journey.Has(Restoration.Observatory)?"Enter   Upper tower":Ending && journey.Has(Restoration.Watch)?"Enter   Observatory":journey.Has(Restoration.Home)?"Enter   Revisit landing":canContinue?"Enter   Continue":"Enter   Begin";
        }
        bool Click(Entity button,bool edge){Vector2 p;if(!edge || button==null || !Input.GetMouseWorldPosition(out p))return false;var t=button.Translation;return Math.Abs(p.X-t.X)<4.4 && Math.Abs(p.Y-t.Y)<.65;}
        void Start(){
            if(CreditsPage){Scene.LoadScene("Scenes/MainMenu.hazel");return;}
            if(damaged){status.Text="Your save cannot be opened. It has been kept.\nPress N twice to start over.";return;}
            if(!canContinue && !Persist(new Journey()))return;
            // Keep the current session alive if departure cannot be saved.
            if(Ending && !Persist(journey))return;
            Scene.LoadScene(Ending && journey.Has(Restoration.Home)?"Scenes/MainMenu.hazel":Ending && journey.Has(Restoration.Observatory)?"Scenes/UpperTower.hazel":Ending && journey.Has(Restoration.Watch)?"Scenes/Observatory.hazel":journey.ResumeScene);
        }
        bool Persist(Journey candidate){try{SaveData.Write("journey",candidate.Encode());journey=candidate;canContinue=true;damaged=false;return true;}catch(Exception ex){status.Text="Could not save. Check available disk space\nand write access, then try again.";Console.WriteLine("Lightkeeper save: "+ex.Message);return false;}}
        void NewGame(){
            if((canContinue || damaged) && !confirm){confirm=true;status.Text="Start over and replace your save?\nPress N again. Esc cancels.";}
            else if(Persist(new Journey()))Scene.LoadScene("Scenes/Breakwater.hazel");
        }
        void CancelNewGame(){confirm=false;status.Text=damaged?"Your save has been kept.":Story.Objective(journey);}
        void OnUpdate(float dt){
            keys.Poll();bool mouse=Input.IsMouseButtonDown(MouseCode.Left),edge=mouse&&!held;held=mouse;
            if(CreditsPage){if(keys.Advance || keys.Escape || Click(StartButton,edge))Start();return;}
            if(keys.Credits || Click(CreditsButton,edge)){confirm=false;Scene.LoadScene("Scenes/Credits.hazel");}
            else if(keys.New || Click(NewButton,edge))NewGame();
            else if(keys.Escape && confirm)CancelNewGame();
            else if(keys.Advance || Click(StartButton,edge)){confirm=false;Start();}
        }
    }
}
