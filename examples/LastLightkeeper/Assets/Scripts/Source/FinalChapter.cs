using System;
using System.Collections.Generic;
using Hazel;
namespace LastLightkeeper {
    // These two scenes share presentation, not an engine dialogue/puzzle framework.
    public class FinalChapter : Entity {
        public Entity Player,Objective,Context,Place,Inventory,TideLabel,Dialogue,DialoguePanel,SaveLabel;
        public Entity SoundTurn,SoundRestore,SoundHazard,SoundWarning;
        public Prefab BeamAsset,EndAsset;
        public bool Home=false;
        public float Speed=3.25f;
        Journey journey; Controls keys; Rigidbody2DComponent body; LightRig rig;
        TextComponent objective,context,dialogue,footer,saveLabel,warning;
        string[] pages; int page; Action after;
        bool paused,journal,saveBlocked,saveFailed; string notice="";float noticeTime,bob,windTime,grace=2;
        Cell facing=new Cell(1,0);
        readonly HashSet<Cell> walls=new HashSet<Cell>();
        readonly Dictionary<Cell,bool> mirrors=new Dictionary<Cell,bool>{{new Cell(-16,2),true}};
        readonly Cell[] contacts={new Cell(-2,11),new Cell(13,5),new Cell(6,2)};
        TidePhase windPhase;
        Entity wisp,mark;Vector3 wispHome,attack;int wispState;float wispTime=2;
        void OnCreate(){
            keys=new Controls(this);body=Player.GetComponent<Rigidbody2DComponent>();
            objective=Objective.GetComponent<TextComponent>();context=Context.GetComponent<TextComponent>();dialogue=Dialogue.GetComponent<TextComponent>();
            footer=FindEntityByName("DialogueFooter").GetComponent<TextComponent>();saveLabel=SaveLabel.GetComponent<TextComponent>();warning=TideLabel.GetComponent<TextComponent>();
            try{journey=Journey.Decode(SaveData.Read("journey"));}catch(Exception ex){journey=new Journey();saveBlocked=true;notice="Save kept. Return to the title to choose a new game.";noticeTime=20;Console.WriteLine(ex.Message);}
            if(!SaveData.IsPersistent && !journey.Has(Restoration.Observatory))journey=Journey.Decode("LK3|8388223|3|2|1|26|2|2|0");
            if(Home && !SaveData.IsPersistent && !journey.Has(Restoration.TowerBeacon)){journey.Restore(Restoration.TowerEntry);journey.Contacts=7;journey.Restore(Restoration.TowerDrive);journey.Restore(Restoration.TowerBeacon);}
            rig=new LightRig(BeamAsset,EndAsset);
            if(!Home){for(int i=0;;i++){var e=FindEntityByName("OpticalStone"+i);if(e==null)break;walls.Add(CellAt(e));}wisp=FindEntityByName("StormWisp");mark=FindEntityByName("RushMark");wispHome=wisp.Translation;Hide(mark);for(int i=0;i<24;i++)Hide(FindEntityByName("Wind"+i));}
            Apply();Recover(false);ShowDialogue();
            saveLabel.Text=SaveData.IsPersistent?"Autosave on":"Editor practice - resets after Stop";
            Place.GetComponent<TextComponent>().Text=Home?"BREAKWATER - THE MORNING LANDING":"UPPER TOWER - EXPOSED WALK";
            if(Home && !journey.Has(Restoration.Home))Talk(new[]{"ORRIN\nThere you are. The main light caught us just\nshort of the outer reef. Nessa made me come back\nfor the compass before she'd step aboard.","NESSA\nIt's my compass. He borrowed it.\nThere's room for you, Iona. We're all here."});
            else if(!Home && !journey.Has(Restoration.TowerEntry))Talk(new[]{"IONA\nCal marked three drive contacts. Each has\na glass sighting window. The latches will hold.\nFirst, the door: Father's mirror is still bolted up."});
        }
        void OnDestroy(){rig?.Destroy();}
        static Cell CellAt(Entity e){var p=e.Translation;return new Cell((int)Math.Round(p.X),(int)Math.Round(p.Y));}
        static void Hide(Entity e){if(e!=null && e.IsValid)e.Scale=new Vector3(.001f,.001f,1);}
        static float Distance(Vector3 a,Vector3 b){float x=a.X-b.X,y=a.Y-b.Y;return (float)Math.Sqrt(x*x+y*y);}
        static Vector3 Delta(Vector3 a,Vector3 b)=>new Vector3(a.X-b.X,a.Y-b.Y,0);
        bool Near(string name,float radius=1.65f){var e=FindEntityByName(name);if(e==null || !e.IsValid)return false;return Distance(e.Translation,Player.Translation)<radius;}
        void Notice(string text,float seconds=4){if(saveFailed)return;notice=text;noticeTime=seconds;}
        void Sound(Entity e){try{e?.GetComponent<AudioSourceComponent>().Play();}catch(Exception ex){Console.WriteLine("Lightkeeper audio: "+ex.Message);}}
        bool Persist(){
            if(saveBlocked)return false;
            try{SaveData.Write("journey",journey.Encode());saveFailed=false;saveLabel.Text=SaveData.IsPersistent?"Progress saved":"Editor practice";return true;}
            catch(Exception ex){saveLabel.Text="Not saved - retry before quitting";notice="Couldn't save. Retry at a repair post before quitting.";noticeTime=10;saveFailed=true;Console.WriteLine(ex.Message);return false;}
        }
        bool Restore(Restoration step){if(saveBlocked || !journey.Restore(step))return false;Apply();Sound(SoundRestore);Persist();return true;}
        void Remove(string name){var e=FindEntityByName(name);if(e!=null && e.IsValid)e.Destroy();}
        void Apply(){
            if(!Home){
                if(journey.Has(Restoration.TowerEntry)){Remove("EntryGate");FindEntityByName("EntryReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));}
                for(int i=0;i<3;i++)if((journey.Contacts&(1<<i))!=0)FindEntityByName("Contact"+i).GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));
                if(journey.Has(Restoration.TowerDrive)){Remove("MainGate");Hide(wisp);Hide(mark);}
                FindEntityByName("BeaconHalo").Scale=journey.Has(Restoration.TowerBeacon)?new Vector3(1,1,1):new Vector3(.001f,.001f,1);
            }
            objective.Text=!Home && !journey.Has(Restoration.TowerEntry)?"Light the door latch through the western window":Story.Objective(journey);Inventory.GetComponent<TextComponent>().Text=Home?"E: talk or use    Q: notes    Esc: pause":"LONG-FOCUS LANTERN    Space: focus    Aim with movement";
        }
        void Recover(bool hit){
            Player.Translation=FindEntityByName("Checkpoint"+(Home?0:journey.TowerCheckpoint)).Translation;body.LinearVelocity=Vector2.Zero;grace=2;windTime=0;
            if(!Home){wispState=3;wispTime=2;wisp.Translation=wispHome;Hide(mark);}
            if(hit){Sound(SoundHazard);Notice("Back at the repair post. Contacts and repairs kept.");}
        }
        void Talk(string[] text,Action done=null){pages=text;page=0;after=done;journal=false;context.Text="";body.LinearVelocity=Vector2.Zero;ShowDialogue();}
        void ShowDialogue(){dialogue.Text=pages==null?"":pages[page];footer.Text=pages==null?"":""+(page+1==pages.Length?(after!=null?"Continue":"Close"):"Next    "+(page+1)+" of "+pages.Length);Presentation.Dialogue(DialoguePanel,pages!=null,false);}
        void CloseDialogue(){pages=null;ShowDialogue();var callback=after;after=null;callback?.Invoke();}
        void Depart(Restoration step,string scene){var before=journey.Restored;if(!journey.Has(step) && !journey.Restore(step))return;Apply();if(Persist())Scene.LoadScene(scene);else{journey.Restored=before;Apply();}}
        bool Sight(Cell from,Cell to){
            int x=from.X,y=from.Y,dx=Math.Abs(to.X-x),dy=-Math.Abs(to.Y-y),sx=x<to.X?1:-1,sy=y<to.Y?1:-1,err=dx+dy;
            for(int n=0;n<100;n++){if(walls.Contains(new Cell(x,y)))return false;if(x==to.X && y==to.Y)return true;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
            return false;
        }
        void Focus(bool active){
            rig.Begin();if(!Home && active && journey.Has(Restoration.Collimator)){
                var emitter=CellAt(Player);var path=Optics.Trace(emitter,facing,journey.Has(Restoration.TowerEntry)?new Cell(100,100):new Cell(-10,2),mirrors,walls,-20,18,-10,14,40);
                if(journey.Has(Restoration.TowerEntry)){
                    for(int n=0;n<path.Steps.Count;n++){
                        int slot=Array.FindIndex(contacts,c=>c.Equals(path.Steps[n].Position));if(slot<0)continue;
                        path.End=path.Steps[n].Position;path.EndDirection=path.Steps[n].Direction;path.Steps.RemoveRange(n+1,path.Steps.Count-n-1);path.Reached=true;path.Obstructed=false;
                        if(!saveBlocked && (journey.Contacts&(1<<slot))==0){journey.Contacts|=1<<slot;if(journey.Contacts==7)journey.Restore(Restoration.TowerDrive);Apply();Persist();Sound(SoundRestore);Notice(journey.Contacts==7?"Drive steady. The main lantern bay is open.":"Contact latched. It holds after the lamp moves.");}break;
                    }
                }else if(path.Reached && Restore(Restoration.TowerEntry))Notice("Door latch caught. Cross when the walk is clear.",6);
                rig.Draw(path,emitter);
                if(!journey.Has(Restoration.TowerDrive) && wispState!=3){var d=Delta(wisp.Translation,Player.Translation);if(LanternRules.InFocus(d.X,d.Y,facing) && Sight(emitter,CellAt(wisp))){wispState=3;wispTime=2;Hide(mark);Sound(SoundTurn);Notice("The wisp scatters. Move to the next sighting window.",2);}}
            }rig.Finish();
        }
        void Wisp(float dt){
            if(Home || journey.Has(Restoration.TowerDrive))return;
            var p=Player.Translation;wispTime-=dt;
            if(wispState==0 && wispTime<=0 && p.X>-6 && p.Y>1 && grace<=0 && Sight(CellAt(wisp),CellAt(Player))){wispState=1;wispTime=1;attack=p;mark.Translation=new Vector3(p.X,p.Y,.1f);mark.Scale=new Vector3(1.6f,1.6f,1);Sound(SoundWarning);}
            if(wispState==1){wisp.Scale=new Vector3(1.3f,1.3f,1);if(wispTime<=0){wispState=2;wispTime=.4f;}}
            else if(wispState==2){float f=Math.Min(1,dt/Math.Max(.01f,wispTime+dt));var old=wisp.Translation;wisp.Translation+=Delta(attack,old)*f;
                // Swept hit test prevents a large frame from skipping over Iona.
                var move=Delta(wisp.Translation,old);var offset=Delta(p,old);float len=move.X*move.X+move.Y*move.Y;float t=len>0?Math.Max(0,Math.Min(1,(offset.X*move.X+offset.Y*move.Y)/len)):0;
                if(grace<=0 && Distance(p,old+move*t)<.65f){Recover(true);return;}if(wispTime<=0){wispState=3;wispTime=2;Hide(mark);}}
            else if(wispState==3){wisp.Scale=new Vector3(.85f,.85f,1);wisp.Translation+=Delta(wispHome,wisp.Translation)*Math.Min(1,dt*3);if(wispTime<=0){wispState=0;wispTime=1;}}
            else{wisp.Scale=new Vector3(1.2f,1.2f,1);wisp.Translation=new Vector3(wispHome.X,wispHome.Y+.12f*(float)Math.Sin(bob*3),wispHome.Z);}
            if(wispState==1)warning.Text="RUSH - STEP OFF THE MARK";else if(wispState==3)warning.Text="WISP SCATTERED";
        }
        string Prompt(){
            if(Home){foreach(var name in new[]{"Orrin","Nessa","Mara","Cal"})if(Near(name,2))return "E   Talk to "+name;if(Near("RepairBook"))return "E   Read the repair book";if(Near("BoatExit"))return "E   Join the morning boat";}
            else{
                if(Near("TowerNote"))return "E   Read Cal's tower settings";
                if(Near("EntryPad"))return "Face up; focus through the window into the fixed mirror";
                if(Near("MainLight"))return journey.Has(Restoration.TowerDrive)?"E   Start the main light":"Three drive contacts still need power";
                for(int i=0;i<3;i++)if(Near("Aim"+i))return (journey.Contacts&(1<<i))!=0?"Contact latched":i==0?"Face up; focus through the north window":i==1?"Face right; focus through the east window":"Face down; focus through the south window";
                if(Near("RestMarker0") || Near("RestMarker1") || Near("RestMarker2"))return "E   Save at the repair post";
            }return "E   Nearby equipment    Q   Notes    Esc   Pause";
        }
        void Interact(){
            if(Home){
                if(Near("Orrin",2)){Talk(new[]{"ORRIN\nNessa's aboard. Mara's got her foot up.\nI'm leaving the boat here tonight. No more\ntrying that reef with a pocket lamp."});return;}
                if(Near("Nessa",2)){Talk(new[]{"NESSA\nCal and I got down the winch walk.\nHe checked every brake twice. I've offered him\na room ashore, with a roof that stays put."});return;}
                if(Near("Mara",2)){Talk(new[]{"MARA\nYou brought the ledger back. Good.\nOpen it at the steps, before your father finds\nsomething else to put ahead of them.","IONA\nI'll order the timber from shore.\nAnd a handrail. You can check my measurements.","MARA\nFrom a chair, this time."});return;}
                if(Near("Cal",2)){Talk(new[]{"CAL\nThe spare lamp's by the mooring. Three short\nflashes when Orrin returns; I wrote it down\nso nobody has to guess who's out there."});return;}
                if(Near("RepairBook")){Talk(new[]{"FATHER'S REPAIR BOOK\nReplace cliff steps. Check main drive glass.\nIona has put a date against both,\nand Mara's name beside the measurements."});return;}
                if(Near("BoatExit")){Talk(new[]{"IONA\nLeave the mooring lamp running, Cal.\nI'll bring the timber on tomorrow's crossing.","ORRIN\nYour usual seat's dry. Mind Mara's foot.\nReady?"},()=>Depart(Restoration.Home,"Scenes/Ending.hazel"));return;}
            }else{
                if(Near("TowerNote")){Talk(new[]{"CAL'S TOWER SETTINGS\nDoor latch: west window and fixed mirror.\nMain drive: north, east and south glass contacts.\nAny order. A lit contact stays latched.","EXPOSED WALK\nFlashing boards warn of a gust.\nStay behind the west rail until it passes.\nInside: step off the wisp's mark, or focus on it."});return;}
                if(Near("MainLight") && journey.Has(Restoration.TowerDrive)){Talk(new[]{"IONA\nThe drive's holding. Glass clean, brake free.\nThat's the sound I wanted.","ORRIN, BELOW\nIona! The reef's clear all the way out!\nCal's lamp is at the mooring. We're coming in.","IONA\nLeave the landing rope for me.\nI'm coming down."},()=>Depart(Restoration.TowerBeacon,"Scenes/HarborHome.hazel"));return;}
                if(Near("RestMarker0") || Near("RestMarker1") || Near("RestMarker2")){if(Persist())Notice("Progress saved at the repair post.");}
            }
        }
        void OnUpdate(float dt){
            keys.Poll();dt=Math.Min(.1f,Math.Max(0,dt));
            if(keys.Escape){if(pages!=null)CloseDialogue();else{paused=!paused;context.Text="";Presentation.Dialogue(DialoguePanel,paused,paused);}}
            if(paused){body.LinearVelocity=Vector2.Zero;if(keys.Menu && (saveBlocked || Persist()))Scene.LoadScene("Scenes/MainMenu.hazel");if(keys.Advance){paused=false;ShowDialogue();}return;}
            if(keys.Journal && journal && pages!=null)CloseDialogue();
            else if(keys.Journal && pages==null){Talk(new[]{(Home?"THE LANDING":"UPPER TOWER")+"\n"+Story.Objective(journey)+".\nWASD or arrows: move. E: use. Esc: pause.",Home?"THE MORNING BOAT\nOrrin and Nessa at the pier. Mara beside her bench.\nCal at the lamp table. The boat leaves from\nthe southern mooring when you're ready.":"MAIN DRIVE\nThree glass contacts, three sighting posts.\nLatches hold; choose their order. Stone columns\nblock light and the wisp's view. Space: focus."});journal=true;}
            if(pages!=null){body.LinearVelocity=Vector2.Zero;if(keys.Advance || keys.Interact){if(++page>=pages.Length)CloseDialogue();else ShowDialogue();}return;}
            noticeTime=Math.Max(0,noticeTime-dt);grace=Math.Max(0,grace-dt);bob+=dt;
            float x=(Input.IsKeyDown(KeyCode.D)||Input.IsKeyDown(KeyCode.Right)?1:0)-(Input.IsKeyDown(KeyCode.A)||Input.IsKeyDown(KeyCode.Left)?1:0);
            float y=(Input.IsKeyDown(KeyCode.W)||Input.IsKeyDown(KeyCode.Up)?1:0)-(Input.IsKeyDown(KeyCode.S)||Input.IsKeyDown(KeyCode.Down)?1:0);
            if(x!=0)facing=new Cell(x>0?1:-1,0);else if(y!=0)facing=new Cell(0,y>0?1:-1);
            bool focus=Input.IsKeyDown(KeyCode.Space);var motion=new Vector2(x,y);float length=motion.Length();body.LinearVelocity=length>0?motion*(Speed*(focus?.5f:1)/length):Vector2.Zero;
            FindEntityByName("IonaCoat").LocalTranslation=new Vector3(0,length>0?.025f*(float)Math.Sin(bob*12):0,.02f);
            var p=Player.Translation;warning.Text="";
            if(Home){if(p.X<-13 || p.X>9 || p.Y<-8 || p.Y>8)Recover(true);}
            else{
                if(p.X<-18.6f || p.X>16.6f || p.Y<-8.6f || p.Y>12.6f){Recover(true);return;}
                windTime+=dt;var phase=Tide.Phase(windTime);if(phase!=windPhase){windPhase=phase;if(phase==TidePhase.Warning && p.Y<0)Sound(SoundWarning);}
                for(int i=0;i<24;i++)FindEntityByName("Wind"+i).Scale=phase==TidePhase.Surge?new Vector3(1,1,1):phase==TidePhase.Warning && (int)(bob*6)%2==0?new Vector3(.65f,.65f,1):new Vector3(.001f,.001f,1);
                if(p.Y<0){warning.Text=phase==TidePhase.Warning?"GUST COMING - WEST RAIL":phase==TidePhase.Surge?"EXPOSED WALK UNSAFE":"WALK CLEAR";if(grace<=0 && phase==TidePhase.Surge && p.X>-13.5f && p.X<-10.5f){Recover(true);return;}}
                int checkpoint=journey.Has(Restoration.TowerEntry) && p.Y>1?journey.Contacts>0 && p.X<-3 && p.Y>7?2:1:0;
                if(checkpoint>journey.TowerCheckpoint){journey.TowerCheckpoint=checkpoint;if(Persist())Notice("Repair post reached. Checkpoint saved.");}
                Wisp(dt);
            }
            if(keys.Interact)Interact();if(pages==null)Focus(focus);
            Place.GetComponent<TextComponent>().Text=Home?"BREAKWATER - THE MORNING LANDING":p.Y>0?"UPPER TOWER - MAIN DRIVE":"UPPER TOWER - EXPOSED WALK";
            context.Text=saveFailed?notice:pages!=null?"":noticeTime>0?notice:Prompt();
        }
    }
}
