using System;
using System.Collections.Generic;
using Hazel;
namespace LastLightkeeper {
    public class CliffChapter : Entity {
        public bool Caves=false;
        public Entity Player,Camera,Objective,Context,Place,Inventory,TideLabel,Dialogue,DialoguePanel,SaveLabel;
        public Entity SoundTurn,SoundRestore,SoundHazard,SoundWarning;
        public Prefab BeamAsset,EndAsset;
        public float Speed=3.25f;
        Journey journey;Controls keys;Rigidbody2DComponent body;LightRig rig;
        TextComponent objective,context,dialogue,saveLabel,footer;
        string[] pages;int page;Action after;
        bool paused,saveBlocked,journal;float noticeTime,grace,bob;
        string notice="";bool saveFailed;Cell facing=new Cell(1,0);
        readonly HashSet<Cell> walls=new HashSet<Cell>();
        readonly List<Entity>[] slats={new List<Entity>(),new List<Entity>(),new List<Entity>()};
        Entity wisp,mark,mara;int maraStep;
        readonly Vector3[] escapeRoute={new Vector3(11,-4,.6f),new Vector3(11,-3,.6f),new Vector3(9,-3,.6f),new Vector3(8,-6,.6f),new Vector3(-8,-6,.6f),new Vector3(-16,-6,.6f)};Vector3 wispHome,attack;int wispState;float wispTime;
        void OnCreate(){
            keys=new Controls();body=Player.GetComponent<Rigidbody2DComponent>();
            objective=Objective.GetComponent<TextComponent>();context=Context.GetComponent<TextComponent>();dialogue=Dialogue.GetComponent<TextComponent>();saveLabel=SaveLabel.GetComponent<TextComponent>();footer=FindEntityByName("DialogueFooter").GetComponent<TextComponent>();
            try{journey=Journey.Decode(SaveData.Read("journey"));}catch(Exception ex){journey=new Journey();saveBlocked=true;Notice("Save kept. Return to the title to choose a new game.",20);Console.WriteLine(ex.Message);}
            // Editor-only direct chapter practice; durable saves are never modified.
            if(!SaveData.IsPersistent && !journey.Has(Restoration.Beacon)){
                journey=Journey.Decode("LK1|127|3|2|1");
                if(Caves){journey.Restore(Restoration.Focus);journey.Restore(Restoration.Drain);journey.Restore(Restoration.Cistern);journey.CaveCheckpoint=1;}
            }
            rig=new LightRig(BeamAsset,EndAsset);
            for(int i=0;;i++){var e=FindEntityByName("Stone"+i);if(e==null)break;walls.Add(CellAt(e));}
            for(int slot=Caves?1:0;slot<(Caves?3:1);slot++)for(int i=0;i<16;i++)slats[slot].Add(FindEntityByName("Shutter"+slot+"Slat"+i));
            if(Caves){wisp=FindEntityByName("StormWisp");mark=FindEntityByName("RushMark");wispHome=wisp.Translation;mara=FindEntityByName("Mara");Hide(mark);}
            Apply();Recover(false);Place.GetComponent<TextComponent>().Text=Caves?(journey.CaveCheckpoint>1?"MARA'S DRY LEDGE":"SEA CAVES - WEST LANDING"):"CLIFF CISTERN - WORKSHOP";ShowDialogue();
            saveLabel.Text=SaveData.IsPersistent?"Autosave on":"Editor practice - resets after Stop";
            if(Caves && !journey.Has(Restoration.CaveGate))Talk(new[]{"IONA\nSomething's caught in the relay. It keeps gathering\nin the same spot. Let's keep the lamp on it."});
            else if(!Caves && !journey.Has(Restoration.Focus))Talk(new[]{"IONA\nHer bench is dry. The lantern sleeve's still here.\nGood. I can test the receivers without the beacon."});
        }
        void OnDestroy(){rig?.Destroy();}
        static Vector3 Delta(Vector3 a,Vector3 b)=>new Vector3(a.X-b.X,a.Y-b.Y,a.Z-b.Z);
        static Cell CellAt(Entity e){var p=e.Translation;return new Cell((int)Math.Round(p.X),(int)Math.Round(p.Y));}
        static void Hide(Entity e){if(e!=null && e.IsValid)e.Scale=new Vector3(.001f,.001f,1);}
        static float Distance(Entity a,Entity b){var p=a.Translation;var q=b.Translation;return (p.X-q.X)*(p.X-q.X)+(p.Y-q.Y)*(p.Y-q.Y);}
        bool Near(string name,float radius=1.65f){var e=FindEntityByName(name);return e!=null && e.IsValid && Distance(Player,e)<radius*radius;}
        void Notice(string text,float seconds=4){if(saveFailed)return;notice=text;noticeTime=seconds;}
        void Sound(Entity e){try{e?.GetComponent<AudioSourceComponent>().Play();}catch(Exception ex){Console.WriteLine("Lightkeeper audio: "+ex.Message);}}
        bool Persist(){if(saveBlocked)return false;try{SaveData.Write("journey",journey.Encode());saveFailed=false;saveLabel.Text=SaveData.IsPersistent?"Progress saved":"Editor practice";return true;}catch(Exception ex){saveLabel.Text="Not saved - retry before quitting";Notice("Couldn't save. Retry at the checkpoint before quitting.",10);saveFailed=true;Console.WriteLine(ex.Message);return false;}}
        bool Restore(Restoration r){if(!journey.Restore(r))return false;Apply();Sound(SoundRestore);Persist();return true;}
        void Talk(string[] text,Action done=null){pages=text;page=0;after=done;journal=false;context.Text="";ShowDialogue();body.LinearVelocity=Vector2.Zero;}
        void ShowDialogue(){dialogue.Text=pages==null?"":pages[page];footer.Text=pages==null?"":"Enter   "+(page+1==pages.Length?(after!=null?"Continue":"Close"):"Next    "+(page+1)+" of "+pages.Length);DialoguePanel.Scale=pages==null?new Vector3(.001f,.001f,1):new Vector3(27,6.8f,1);}
        void CloseDialogue(){pages=null;ShowDialogue();var callback=after;after=null;callback?.Invoke();}
        void Remove(string name){var e=FindEntityByName(name);if(e!=null && e.IsValid)e.Destroy();}
        void Apply(){
            for(int slot=Caves?1:0;slot<(Caves?3:1);slot++){
                int gap=LanternRules.Gap(Caves,slot,journey.Shutter(slot));float x=Caves?(slot==1?-6:10):0;
                for(int i=0;i<16;i++){var e=slats[slot][i];if(e==null)continue;int row=i-8;if(row>=gap)row++;bool opening=Caves && journey.Has(Restoration.Mara) && ((slot==1 && (row==-7 || row==-6)) || (slot==2 && row==-3));e.Translation=new Vector3(opening?-90:x,opening?-90:row,.4f);}
                var arrow=FindEntityByName("SlotArrow"+slot);arrow.Translation=new Vector3(x-.75f,gap,.5f);
            }
            if(journey.Has(Restoration.Focus))Hide(FindEntityByName("LanternSleeve"));
            if(!Caves){
                if(journey.Has(Restoration.Drain)){Remove("DrainGate");FindEntityByName("DrainReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));}
                if(journey.Has(Restoration.Cistern)){Remove("CaveEntranceGate");FindEntityByName("BridgeReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));}
            }else{
                if(journey.Has(Restoration.CaveGate)){Remove("GalleryGate");FindEntityByName("GalleryReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));}
                if(journey.Has(Restoration.Mara))Remove("ReturnGate");
                if(journey.Has(Restoration.Rescue)){Remove("MaraDoor");FindEntityByName("RescueReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));}
            }
            objective.Text=!Caves && journey.Has(Restoration.Cistern)?"Take the eastern stairs into the sea caves":Story.Objective(journey);
            Inventory.GetComponent<TextComponent>().Text=journey.Has(Restoration.Focus)?"LANTERN    Space: focus    Aim with movement":"WRENCH    LANTERN";
        }
        void Recover(bool hit){
            int cp=Caves?(journey.CaveCheckpoint>1?2:1):0;
            Player.Translation=FindEntityByName("Checkpoint"+cp).Translation;body.LinearVelocity=Vector2.Zero;grace=1.5f;
            if(Caves){wisp.Translation=wispHome;wisp.Scale=new Vector3(1,1,1);wispState=0;wispTime=1.5f;Hide(mark);}
            if(hit){Sound(SoundHazard);Notice("Back at the dry ledge. Shutters and repairs kept.");}
        }
        HashSet<Cell> Blocks(){
            var blocked=new HashSet<Cell>(walls);
            if(Caves){LanternRules.ShutterBlocks(blocked,-6,LanternRules.Gap(true,1,journey.Shutter(1)));LanternRules.ShutterBlocks(blocked,10,LanternRules.Gap(true,2,journey.Shutter(2)));}
            else LanternRules.ShutterBlocks(blocked,0,LanternRules.Gap(false,0,journey.Shutter(0)));
            foreach(var name in new[]{"DrainGate","CaveEntranceGate","GalleryGate","MaraDoor"}){var gate=FindEntityByName(name);if(gate!=null && gate.IsValid)blocked.Add(CellAt(gate));}
            var returnGate=FindEntityByName("ReturnGate");if(returnGate!=null && returnGate.IsValid){blocked.Add(new Cell(-6,-6));blocked.Add(new Cell(-6,-7));}
            return blocked;
        }
        void Focus(bool active){
            rig.Begin();
            if(active && journey.Has(Restoration.Focus)){
                var emitter=CellAt(Player);var mirrors=new Dictionary<Cell,bool>();
                Cell target;
                if(Caves){
                    mirrors.Add(new Cell(8,3),false);mirrors.Add(new Cell(8,-3),false);
                    target=journey.Has(Restoration.CaveGate)?new Cell(13,-3):new Cell(-1,6);
                }else{target=journey.Has(Restoration.Drain)?new Cell(12,6):new Cell(8,0);mirrors.Add(new Cell(8,3),true);mirrors.Add(new Cell(8,6),true);}
                var path=Optics.Trace(emitter,facing,target,mirrors,Blocks(),-22,22,-12,12,24);
                rig.Draw(path,emitter);
                if(path.Reached && !saveBlocked){
                    if(!Caves){if(!journey.Has(Restoration.Drain)){if(Restore(Restoration.Drain))Notice("Drain latch caught. The inspection walk is open.");}else if(Restore(Restoration.Cistern))Notice("Cliff bridge released. Take the stairs to the caves.",6);}
                    else if(!journey.Has(Restoration.CaveGate)){if(Restore(Restoration.CaveGate)){Notice("Gallery door open. A dry ledge lies beyond.",5);}}
                    else if(Restore(Restoration.Rescue))Notice("Mara's door is open. Go through the eastern shutter.",6);
                }
                if(Caves && wispState!=3){var delta=Delta(wisp.Translation,Player.Translation);if(LanternRules.InFocus(delta.X,delta.Y,facing)){
                    var test=Optics.Trace(emitter,facing,CellAt(wisp),new Dictionary<Cell,bool>(),Blocks(),-20,20,-10,10);
                    if(test.Reached){wispState=3;wispTime=2;Hide(mark);Notice("The wisp scatters. Keep moving.",2);}
                }}
            }
            rig.Finish();
        }
        void Wisp(float dt){
            var p=Player.Translation;wispTime-=dt;
            if(wispState==0 && wispTime<=0 && p.X>-5 && p.X<5 && p.Y>-4 && p.Y<7 && grace<=0){wispState=1;wispTime=.8f;attack=p;mark.Translation=new Vector3(p.X,p.Y,.1f);mark.Scale=new Vector3(1.6f,1.6f,1);Sound(SoundWarning);}
            if(wispState==1){wisp.Scale=new Vector3(1.3f,1.3f,1);if(wispTime<=0){wispState=2;wispTime=.3f;}}
            else if(wispState==2){float f=Math.Min(1,dt/Math.Max(.01f,wispTime+dt));wisp.Translation+=(Delta(attack,wisp.Translation))*f;if(grace<=0 && Distance(Player,wisp)<.65f){Recover(true);return;}if(wispTime<=0){wispState=3;wispTime=1.5f;Hide(mark);}}
            else if(wispState==3){wisp.Scale=new Vector3(.85f,.85f,1);wisp.Translation+=(Delta(wispHome,wisp.Translation))*Math.Min(1,dt*3);if(wispTime<=0){wispState=0;wispTime=1;}}
            else{wisp.Scale=new Vector3(1,1,1);wisp.LocalTranslation=new Vector3(wispHome.X,wispHome.Y+.12f*(float)Math.Sin(bob*3),wispHome.Z);}
            TideLabel.GetComponent<TextComponent>().Text=wispState==1?"RUSH - STEP ASIDE":wispState==3?"WISP SCATTERED":"";
        }
        string Prompt(){
            if(!Caves){
                if(Near("LanternSleeve") && !journey.Has(Restoration.Focus))return "E   Fit the lantern's focus sleeve";
                if(Near("Handle0"))return "E   Slide cistern shutter";
                if(Near("FocusPad0"))return journey.Has(Restoration.Drain)?"Drain latch holds. The raised walk is open":"Face right. Hold Space to light the drain receiver";
                if(Near("FocusPad1"))return journey.Has(Restoration.Cistern)?"Bridge released. Take the eastern stairs":"Face right. Hold Space into the mirror route";
                if(Near("CaveExit"))return journey.Has(Restoration.Cistern)?"E   Descend into the sea caves":"Bridge locked. Light the upper receiver.";
                if(Near("WorkshopNote"))return "E   Read Mara's bench note";
            }else{
                if(Near("Handle1"))return "E   Slide gallery shutter";
                if(Near("Handle2"))return "E   Slide eastern shutter";
                if(Near("FocusPad2"))return journey.Has(Restoration.CaveGate)?"Gallery door open":"Face right; hold Space toward the gallery receiver";
                if(Near("FocusPad3"))return journey.Has(Restoration.Rescue)?"Mara's door is open":"Face right; hold Space into the fixed mirror route";
                if(Near("Mara",2.1f))return "E   Talk to Mara";
                if(Near("DryExit"))return journey.Has(Restoration.Mara)?"E   Take the dry passage with Mara":"Find Mara before leaving";
                if(Near("SurveyLedger"))return "E   Read the survey ledger";
            }
            if(Near("RestMarker"))return "E   Save at the dry ledge";
            return "E   Nearby equipment    Q   Notes    Esc   Pause";
        }
        void Interact(){
            if(!Caves && Near("LanternSleeve") && !journey.Has(Restoration.Focus)){
                if(Restore(Restoration.Focus))Talk(new[]{"LANTERN\nHold Space to focus. Aim by moving first.\nThe marked tiles line up with the receivers."});return;
            }
            for(int slot=Caves?1:0;slot<(Caves?3:1);slot++)if(Near("Handle"+slot)){journey.MoveShutter(slot);Apply();Persist();Sound(SoundTurn);int gap=LanternRules.Gap(Caves,slot,journey.Shutter(slot));Notice("Opening at the "+(gap==(slot==1?6:3)?"upper":gap==(slot==1?3:0)?"middle":"lower")+" notch.",2);return;}
            if(!Caves && Near("CaveExit")){if(journey.Has(Restoration.Cistern)){journey.CaveCheckpoint=Math.Max(1,journey.CaveCheckpoint);if(Persist())Scene.LoadScene("Scenes/SeaCaves.hazel");}return;}
            if(Caves && Near("DryExit")){if(journey.Has(Restoration.Mara)){
                Restore(Restoration.Watch);Talk(new[]{"MARA\nOrrin's boat. They've made it off the ledge.\nBut look at the observatory. Someone's waving.","IONA\nBoat first. Then I'll take your ledger up.\nFather still owes you a new set of steps."},()=>{if(Persist())Scene.LoadScene("Scenes/CliffDawn.hazel");});}return;}
            if(Caves && Near("Mara",2.1f)){
                if(journey.Has(Restoration.Rescue) && Restore(Restoration.Mara))Talk(new[]{"MARA\nIona! Lower that lamp, will you?\nI tried the steps. My ankle won't take them.","IONA\nOrrin's coming round in the boat.\nWe'll take the dry passage. Have you got the key?","MARA\nFront pocket. It releases the lower shutters.\nTake the ledger too. Two people stayed up at the\nobservatory when the roof went. They need help."});
                else Talk(journey.Has(Restoration.Mara)?new[]{"MARA\nThe dry passage goes west under the gallery.\nI'll follow the rail. You check the landing."}:new[]{"MARA, THROUGH THE DOOR\nThe door's jammed. Try the receiver by the window.\nAnd hurry. I'm sitting in the only dry patch."});return;
            }
            if(Near("RestMarker")){if(Persist())Notice("Progress saved at the dry ledge.");return;}
            if(!Caves && Near("WorkshopNote"))Talk(new[]{"MARA'S BENCH NOTE\nHandle moves the opening one notch.\nFloor receiver: drain. Upper receiver: bridge.\nBoth latches hold after the lamp moves."});
            if(Caves && Near("SurveyLedger"))Talk(new[]{"SURVEY LEDGER\nCrew waiting on the survey ledge.\nTwo still at the observatory. Outer shutter twisted.\nMara: winch key with me. Going down for help."});
        }
        void OnUpdate(float dt){
            keys.Poll();dt=Math.Min(.1f,Math.Max(0,dt));
            if(keys.Escape){if(pages!=null)CloseDialogue();else{paused=!paused;context.Text="";dialogue.Text=paused?"PAUSED":"";footer.Text=paused?"Enter   Resume     M   Title":"";DialoguePanel.Scale=paused?new Vector3(27,6.8f,1):new Vector3(.001f,.001f,1);}}
            if(paused){body.LinearVelocity=Vector2.Zero;if(keys.Menu)Scene.LoadScene("Scenes/MainMenu.hazel");if(keys.Advance){paused=false;ShowDialogue();}return;}
            if(keys.Journal && journal && pages!=null)CloseDialogue();
            else if(keys.Journal && pages==null){Talk(new[]{"CLIFF NOTES\n"+Story.Objective(journey)+".\nWASD or arrows: move. E: use nearby equipment.\nHold Space to focus. Aim by moving first.",Caves?"SEA CAVES\nGallery receiver on the northern wall.\nMove off the wisp's mark, or focus the lamp on it.\nThe dry western passage leads to the landing.":"CISTERN\nWorkshop west. Drain receiver beside the pool.\nInspection walk above it; stairs at the east end.\nBoth receiver latches hold after the lamp moves."});journal=true;}
            if(pages!=null){body.LinearVelocity=Vector2.Zero;if(keys.Advance || keys.Interact){if(++page>=pages.Length)CloseDialogue();else ShowDialogue();}return;}
            bob+=dt;noticeTime=Math.Max(0,noticeTime-dt);grace=Math.Max(0,grace-dt);
            float x=(Input.IsKeyDown(KeyCode.D)||Input.IsKeyDown(KeyCode.Right)?1:0)-(Input.IsKeyDown(KeyCode.A)||Input.IsKeyDown(KeyCode.Left)?1:0);
            float y=(Input.IsKeyDown(KeyCode.W)||Input.IsKeyDown(KeyCode.Up)?1:0)-(Input.IsKeyDown(KeyCode.S)||Input.IsKeyDown(KeyCode.Down)?1:0);
            if(x!=0)facing=new Cell(x>0?1:-1,0);else if(y!=0)facing=new Cell(0,y>0?1:-1);
            bool focus=Input.IsKeyDown(KeyCode.Space) && journey.Has(Restoration.Focus);
            var motion=new Vector2(x,y);float length=motion.Length();body.LinearVelocity=length>0?motion*(Speed*(focus?.5f:1)/length):Vector2.Zero;
            var coat=FindEntityByName("IonaCoat");coat.LocalTranslation=new Vector3(0,length>0?.025f*(float)Math.Sin(bob*12):0,.02f);
            var p=Player.Translation;if(p.X<-20 || p.X>20 || p.Y<-10 || p.Y>10){Recover(true);return;}
            if(Caves && p.X>5 && journey.Has(Restoration.CaveGate) && journey.CaveCheckpoint<2){journey.CaveCheckpoint=2;if(Persist())Notice("Eastern ledge reached. Checkpoint saved.");}
            if(keys.Interact)Interact();Focus(focus);if(Caves && pages==null){Wisp(dt);if(journey.Has(Restoration.Mara) && maraStep<escapeRoute.Length){var target=escapeRoute[maraStep];var delta=Delta(target,mara.Translation);float distance=(float)Math.Sqrt(delta.X*delta.X+delta.Y*delta.Y);if(distance<.1f)maraStep++;else mara.Translation+=delta*(Math.Min(distance,dt*2.8f)/distance);}}
            Place.GetComponent<TextComponent>().Text=Caves?(p.X>6?"MARA'S DRY LEDGE":p.X>-6?"SEA CAVES - RELAY GALLERY":"SEA CAVES - WEST LANDING"):p.X<0?"CLIFF CISTERN - WORKSHOP":"CLIFF CISTERN - INSPECTION WALK";
            context.Text=saveFailed || noticeTime>0?notice:Prompt();
        }
    }
}
