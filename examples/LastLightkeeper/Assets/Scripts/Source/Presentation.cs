using System;
using Hazel;
namespace LastLightkeeper {
    public sealed class Controls {
        private bool e,escape,q;
        private readonly Entity settings,leave,quit,dialogue;
        public bool Interact,Advance,Escape,Journal,Menu;
        public Controls(Entity owner){dialogue=owner.FindEntityByName("DialoguePanel");settings=owner.FindEntityByName("Settings screen");leave=owner.FindEntityByName("Leave confirmation");quit=owner.FindEntityByName("Quit confirmation");Poll();}
        void Open(Entity screen){screen.GetComponent<UICanvasComponent>().Open();}
        public void Poll(){
            UIEvent value;bool advance=false,pause=false,journal=false,menu=false;
            while(GameUI.TryConsumeEvent(out value)) {
                if(value.Kind==UIEventKind.Cancel){
                    if(value.Screen!=null&&(value.Screen.ID==leave.ID||value.Screen.ID==quit.ID))value.Screen.GetComponent<UICanvasComponent>().Close();
                    else pause=true;
                }
                if(value.Kind!=UIEventKind.Activated)continue;
                switch(value.Action){
                    case "advance":case "resume":advance=true;break;
                    case "pause":pause=true;break;
                    case "journal":journal=true;break;
                    case "menu":menu=true;break;
                    case "settings":Open(settings);break;
                    case "request-menu":Open(leave);break;
                    case "request-quit":Open(quit);break;
                    case "quit":Game.Exit();break;
                    case "dismiss":value.Screen.GetComponent<UICanvasComponent>().Close();break;
                }
            }
            bool a=GameUI.IsKeyDown(KeyCode.E),f=GameUI.IsKeyDown(KeyCode.Escape),g=GameUI.IsKeyDown(KeyCode.Q);
            Interact=Input.IsKeyDown(KeyCode.E)&&a&&!e;Advance=advance;
            // A nonmodal UI Cancel owns its frame; do not reopen pause when
            // it releases HUD keyboard focus or when a held key is rearmed.
            Escape=pause||(Input.IsKeyDown(KeyCode.Escape)&&GameUI.FocusScope==0&&f&&!escape);
            Journal=journal||(!GameUI.IsPointerCaptured&&((GameUI.FocusScope==0&&Input.IsKeyDown(KeyCode.Q))||GameUI.FocusScope==dialogue.ID)&&g&&!q);
            Menu=menu; // Returning to title is an explicit confirmed screen command.
            e=a;escape=f;q=g;
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
        public Entity SettingsScreen,NewGameConfirmation,NewLabel,CreditsLabel,ControlsHint;
        public bool Ending=false,CreditsPage=false;
        private TextComponent status;
        private bool confirm,canContinue,damaged;

        private Journey journey;
        void OnCreate(){
            status=Status.GetComponent<TextComponent>();
            try {var raw=SaveData.Read("journey");journey=Journey.Decode(raw);canContinue=!String.IsNullOrEmpty(raw);}
            catch(Exception ex){journey=new Journey();damaged=true;status.Text="Your save could not be read. It has been kept.\nChoose New game or Credits.";Console.WriteLine("Lightkeeper save: "+ex.Message);}
            if(CreditsPage){
                Heading.GetComponent<TextComponent>().Text="THE LAST\nLIGHTKEEPER";
                Subtitle.GetComponent<TextComponent>().Text="Art and sound: Kenney (CC0).\nEngine: Hazel. Built with the Hazel community.";
                status.Text="All six asset packs and their licenses travel\nwith the game. Full notices: Assets/Licenses.\nAn independent game; no Kenney endorsement.";
                StartLabel.GetComponent<TextComponent>().Text="Title";
                NewButton.GetComponent<UIRectComponent>().Visible=false;CreditsButton.GetComponent<UIRectComponent>().Visible=false;
                NewLabel.GetComponent<TextComponent>().Text="";CreditsLabel.GetComponent<TextComponent>().Text="";
                ControlsHint.GetComponent<TextComponent>().Text="Accept or Cancel: return to title";return;
            }
            if(Ending){
                bool finished=journey.Has(Restoration.Home),survey=journey.Has(Restoration.Observatory);
                Heading.GetComponent<TextComponent>().Text=finished?"THE MORNING\nBOAT":survey?"THE SURVEY\nCREW":journey.Has(Restoration.Watch)?"THE DRY\nWAY HOME":"THE LOWER\nBEACON";
                Subtitle.GetComponent<TextComponent>().Text=finished?"The main light is steady. Everyone is aboard.\nTomorrow's crossing brings timber for the steps.":survey?"Cal can reach the landing.\nNessa is holding the tower winch brake.":journey.Has(Restoration.Watch)?"Mara is safe. Orrin has reached the survey ledge.\nTwo people remain at the observatory.":"Orrin can get through the reef.\nMara's lamp is moving beneath the cistern.";
                status.Text=finished?"Journey saved. Continue revisits the landing.\nCredits is available from the menu.":survey?"Cal's settings are in the ledger.\nTake the winch walk to the upper tower.":journey.Has(Restoration.Watch)?"Mara's ledger is dry.\nTake it up the north path.":"The cliff path is open.";
            } else if(!damaged)status.Text=canContinue?Story.Objective(journey):"Three nights without a beacon.";
            if(!Ending && !damaged){
                if(journey.Has(Restoration.Home))Subtitle.GetComponent<TextComponent>().Text="Everyone reached the morning boat.\nTomorrow's crossing brings timber for the steps.";
                else if(journey.Has(Restoration.TowerBeacon))Subtitle.GetComponent<TextComponent>().Text="Everyone is back at the landing.\nOrrin is ready for the morning crossing.";
                else if(journey.Has(Restoration.TowerDrive))Subtitle.GetComponent<TextComponent>().Text="The drive is steady.\nIona can reach the main lantern.";
                else if(journey.Has(Restoration.TowerEntry))Subtitle.GetComponent<TextComponent>().Text="Iona is at the upper tower.\nThe main drive still needs light.";
            }
            StartButton.GetComponent<UIButtonComponent>().Interactable=!damaged;
            StartLabel.GetComponent<TextComponent>().Text=damaged?"Save unavailable":Ending && journey.Has(Restoration.Home)?"Title":Ending && journey.Has(Restoration.Observatory)?"Upper tower":Ending && journey.Has(Restoration.Watch)?"Observatory":journey.Has(Restoration.Home)?"Revisit landing":canContinue?"Continue":"Begin";
        }
        void Start(){
            if(CreditsPage){Scene.LoadScene("Scenes/MainMenu.hazel");return;}
            if(damaged){status.Text="Your save cannot be opened. It has been kept.\nChoose New game to request replacement.";return;}
            if(!canContinue && !Persist(new Journey()))return;
            // Keep the current session alive if departure cannot be saved.
            if(Ending && !Persist(journey))return;
            Scene.LoadScene(Ending && journey.Has(Restoration.Home)?"Scenes/MainMenu.hazel":Ending && journey.Has(Restoration.Observatory)?"Scenes/UpperTower.hazel":Ending && journey.Has(Restoration.Watch)?"Scenes/Observatory.hazel":journey.ResumeScene);
        }
        bool Persist(Journey candidate){try{SaveData.Write("journey",candidate.Encode());journey=candidate;canContinue=true;damaged=false;return true;}catch(Exception ex){status.Text="Could not save. Check available disk space\nand write access, then try again.";Console.WriteLine("Lightkeeper save: "+ex.Message);return false;}}
        void NewGame(){
            if((canContinue || damaged) && !confirm){confirm=true;NewGameConfirmation.GetComponent<UICanvasComponent>().Open();}
            else if(Persist(new Journey()))Scene.LoadScene("Scenes/Breakwater.hazel");
        }
        void CancelNewGame(){NewGameConfirmation.GetComponent<UICanvasComponent>().Close();confirm=false;status.Text=damaged?"Your save has been kept.":Story.Objective(journey);}
        void OnUpdate(float dt){
            UIEvent value;
            while(GameUI.TryConsumeEvent(out value)) {
                if(value.Kind==UIEventKind.Cancel) {
                    if(confirm)CancelNewGame();else if(CreditsPage)Start();
                    continue;
                }
                if(value.Kind!=UIEventKind.Activated)continue;
                switch(value.Action){
                    case "settings":SettingsScreen.GetComponent<UICanvasComponent>().Open();break;
                    case "quit":Game.Exit();break;
                    case "credits":confirm=false;Scene.LoadScene("Scenes/Credits.hazel");break;
                    case "new":case "new-confirm":NewGame();break;
                    case "dismiss":CancelNewGame();break;
                    case "start":confirm=false;Start();break;
                }
            }
        }

    }
    static class Presentation {
        public static void Dialogue(Entity panel,bool visible,bool pause){
            var dialogue=panel.GetComponent<UICanvasComponent>();
            var paused=panel.FindEntityByName("Pause screen").GetComponent<UICanvasComponent>();
            if(pause){dialogue.Close();if(visible)paused.Open();else paused.Close();}
            else {paused.Close();if(visible)dialogue.Open();else dialogue.Close();}
        }
        public static void HideUI(Entity entity){Dialogue(entity,false,false);}
    }
}
