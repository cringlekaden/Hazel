using System;
using System.Collections.Generic;
using Hazel;
namespace LastLightkeeper {
    public class Breakwater : Entity {
        public Entity Player,Camera,HarborMirror,HarborGate,Pump,Lens,Socket,Receiver,Tower,Wrench;
        public Entity MirrorOne,MirrorTwo,MirrorThree,ServiceDoor,ServiceReceiver;
        public Entity Objective,Context,Place,Inventory,TideLabel,Dialogue,DialoguePanel,SaveLabel;
        public Entity SoundTurn,SoundRestore,SoundHazard,SoundWarning;
        public Prefab BeamAsset,EndAsset;
        public float Speed=3.25f,InteractionRadius=1.5f;
        private Journey journey;
        private LightRig rig;
        private Controls keys;
        private Rigidbody2DComponent body;
        private Entity[] interactables,mirrors,surges,warningBoards;
        private string[] pages;
        private int page,nearest=-1;
        private Action afterDialogue;
        private bool ready,paused,journal,saveBlocked,lightDirty=true;
        private int shownPhase=-1;
        private float clock,grace,noticeTime,bob;
        private Entity coat;
        private TextComponent objective,context,place,inventory,tideLabel,dialogue,saveLabel,footer;
        private string notice="";
        private bool saveFailed;
        private readonly HashSet<Cell> stone=new HashSet<Cell>();
        void OnCreate(){
            keys=new Controls();body=Player.GetComponent<Rigidbody2DComponent>();
            objective=Objective.GetComponent<TextComponent>();context=Context.GetComponent<TextComponent>();place=Place.GetComponent<TextComponent>();
            inventory=Inventory.GetComponent<TextComponent>();tideLabel=TideLabel.GetComponent<TextComponent>();dialogue=Dialogue.GetComponent<TextComponent>();saveLabel=SaveLabel.GetComponent<TextComponent>();footer=FindEntityByName("DialogueFooter").GetComponent<TextComponent>();
            try{journey=Journey.Decode(SaveData.Read("journey"));}catch(Exception ex){journey=new Journey();saveBlocked=true;Notice("Save kept. Return to the title to choose a new game.",12);Console.WriteLine("Lightkeeper save: "+ex.Message);}
            mirrors=new[]{MirrorOne,MirrorTwo,MirrorThree};
            interactables=new[]{HarborMirror,FindEntityByName("Orrin"),Wrench,FindEntityByName("MaraNote"),Pump,Lens,Socket,MirrorOne,MirrorTwo,MirrorThree,Tower,FindEntityByName("Archive"),FindEntityByName("Cache")};
            coat=FindEntityByName("IonaCoat");
            saveLabel.Text=SaveData.IsPersistent?"Autosave on":"Editor practice - resets after Stop";
            for(int i=0;i<5;i++){var e=FindEntityByName("Occluder"+i);if(e!=null)stone.Add(CellAt(e));}
            warningBoards=new Entity[10];for(int i=0;i<10;i++)warningBoards[i]=FindEntityByName("Tide warning "+(-18+i%5)+","+(i<5?6:11));
            surges=new Entity[20];for(int i=0;i<20;i++)surges[i]=FindEntityByName("Surge"+i);
            rig=new LightRig(BeamAsset,EndAsset);
            ApplyJourney();Checkpoint(false);place.Text=journey.Checkpoint==0?"HARBOR LANDING":journey.Checkpoint==2?"TIDE WORKS":"SALT GROVE";SetDialogue();
            if(journey.Restored==Restoration.None)Talk(Story.Arrival);
            else Notice("Resumed: "+Story.Objective(journey),4);
        }
        void OnDestroy(){rig?.Destroy();}
        static Cell CellAt(Entity e){var p=e.Translation;return new Cell((int)Math.Round(p.X),(int)Math.Round(p.Y));}
        static float Distance(Entity a,Entity b){var p=a.Translation;var q=b.Translation;return (p.X-q.X)*(p.X-q.X)+(p.Y-q.Y)*(p.Y-q.Y);}
        static void Hide(Entity e){if(e!=null && e.IsValid)e.Scale=new Vector3(.001f,.001f,1);}
        void Notice(string text,float duration=3){if(saveFailed)return;notice=text;noticeTime=duration;}
        void Sound(Entity e){try{if(e!=null)e.GetComponent<AudioSourceComponent>().Play();}catch(Exception ex){Console.WriteLine("Lightkeeper audio: "+ex.Message);}}
        bool Persist(){
            if(saveBlocked)return false;
            try{SaveData.Write("journey",journey.Encode());saveFailed=false;saveLabel.Text=SaveData.IsPersistent?"Progress saved":"Editor practice";return true;}
            catch(Exception ex){saveLabel.Text="Not saved - retry before quitting";Notice("Cannot save. Keep this session open; retry at the pump.",8);saveFailed=true;Console.WriteLine("Lightkeeper save: "+ex.Message);return false;}
        }
        bool Restore(Restoration step){if(!journey.Restore(step))return false;Sound(SoundRestore);ApplyJourney();Persist();return true;}
        void ApplyJourney(){
            lightDirty=true;
            HarborMirror.LocalRotation=new Vector3(0,0,journey.HarborSlash?0:(float)Math.PI/2);
            for(int i=0;i<3;i++)mirrors[i].LocalRotation=new Vector3(0,0,(journey.Mirrors&(1<<i))!=0?0:(float)Math.PI/2);
            if(journey.Has(Restoration.Harbor) && HarborGate!=null && HarborGate.IsValid)HarborGate.Destroy();
            if(journey.Has(Restoration.CourtUnlocked) && ServiceDoor!=null && ServiceDoor.IsValid)ServiceDoor.Destroy();
            if(journey.Has(Restoration.CourtUnlocked))ServiceReceiver.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Wrench))Hide(Wrench);
            if(journey.Has(Restoration.Lens))Hide(Lens);
            if(journey.Has(Restoration.Pump))Pump.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Mounted))Socket.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",87));
            if(journey.Has(Restoration.Relay))Receiver.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Harbor))FindEntityByName("HarborReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Cache))Hide(interactables[12]);
            if(journey.Has(Restoration.Beacon)){FindEntityByName("BeaconHalo").Scale=new Vector3(1,1,1);FindEntityByName("Lantern mechanism").GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",87));}
            objective.Text=Story.Objective(journey);
            inventory.Text=(journey.Has(Restoration.Wrench)?"WRENCH  ":"")+(journey.Has(Restoration.Lens)?"LENS  ":"")+(journey.Has(Restoration.Cache)?"SEA GLASS":"");
        }
        // Stable IDs are loaded from the shipped sheet identity table, not regenerated.
        static Sprite Region(string pack,int tile)=>Art.Region(pack,tile);
        void Talk(string[] text,Action done=null){pages=text;page=0;afterDialogue=done;context.Text="";journal=false;SetDialogue();}
        void SetDialogue(){dialogue.Text=pages==null?"":pages[page];footer.Text=pages==null?"":"Enter   "+(page+1==pages.Length?(afterDialogue!=null?"Continue":"Close"):"Next    "+(page+1)+" of "+pages.Length);Presentation.Dialogue(DialoguePanel,pages!=null,false);}
        void CloseDialogue(){pages=null;SetDialogue();var callback=afterDialogue;afterDialogue=null;callback?.Invoke();}
        void Checkpoint(bool announce){
            var marker=FindEntityByName("Checkpoint"+journey.Checkpoint);Player.Translation=marker.Translation;body.LinearVelocity=Vector2.Zero;grace=.65f;
            if(announce){Notice("Back at the pump rail. Repairs kept.",4);Sound(SoundHazard);}
        }
        string Prompt(int i){
            switch(i){case 0:return journey.Has(Restoration.Harbor)?"Harbor signal steady":"E   Turn the harbor mirror";
                case 1:return "E   Talk to Orrin";case 2:return journey.Has(Restoration.Wrench)?"":"E   Recover Mara's wrench";
                case 3:return "E   Read Mara's field note";case 4:return journey.Has(Restoration.Pump)?"E   Read the tide log":"E   Repair the sluice";
                case 5:return journey.Has(Restoration.Lens)?"":"E   Recover the lighthouse lens";
                case 6:return journey.Has(Restoration.Mounted)?"E   Read the maintenance plate":"E   Fit the lens";
                case 7:case 8:case 9:return "E   Turn mirror "+(i-6);
                case 10:return "E   Climb to the lower lantern";case 11:return "E   Read Father's repair book";case 12:return journey.Has(Restoration.Cache)?"":"E   Take a piece of sea glass";default:return "";}
        }
        void Interact(int i){
            switch(i){
                case 0:
                    if(journey.Has(Restoration.Harbor)){Notice("The bridge is down. Orrin has a clear landing.");break;}
                    journey.HarborSlash=!journey.HarborSlash;Sound(SoundTurn);ApplyJourney();
                    if(journey.HarborSlash){Restore(Restoration.Harbor);journey.Checkpoint=1;Persist();Notice("Bridge lowered. The grove is across the channel.",5);}
                    else Persist();break;
                case 1:Talk(Story.Ferryman);break;
                case 2:if(!journey.Has(Restoration.Harbor)){Notice("Open the harbor bridge first.");break;}if(Restore(Restoration.Wrench))Talk(new[]{"IONA\nSailcloth round the handle. That's Mara's wrench.\nThis should shift the pump wheel."});break;
                case 3:Talk(Story.Grove);break;
                case 4:
                    if(journey.Has(Restoration.Pump)){Talk(Story.Pump);Persist();break;}
                    if(!journey.Has(Restoration.Wrench)){Notice("The wheel is seized. Mara's wrench is in the eastern grove.",5);break;}
                    Restore(Restoration.Pump);journey.Checkpoint=2;Persist();Talk(Story.Pump);break;
                case 5:
                    if(!journey.Has(Restoration.Pump)){Notice("Drain the platform first. The pump wheel is seized.",5);break;}
                    if(Restore(Restoration.Lens))Talk(Story.Lens);break;
                case 6:
                    if(!journey.Has(Restoration.Lens)){Talk(Story.Relay);break;}
                    if(Restore(Restoration.Mounted))Notice("Lens seated. The court has light.",5);else Talk(Story.Relay);break;
                case 7:case 8:case 9:
                    if(!journey.Has(Restoration.Mounted)){Notice("Recover the lens and fit it in the western socket.",5);break;}
                    journey.Mirrors^=1<<(i-7);Sound(SoundTurn);ApplyJourney();Persist();break;
                case 10:
                    if(!journey.Has(Restoration.Relay)){Notice("The lantern needs power from the mirror court.",5);break;}
                    Restore(Restoration.Beacon);Talk(Story.Ending,()=>{if(Persist())Scene.LoadScene("Scenes/CliffCistern.hazel");});break;
                case 11:Restore(Restoration.Archive);Talk(new[]{"FATHER'S REPAIR BOOK\nPump rail replaced. Kept the bent bolts for spares.\nMara, in the margin: Replace those cliff steps next.","IONA\nHe crossed out the rail. Not the steps.\nI wish he'd listened to her."});break;
                case 12:if(Restore(Restoration.Cache)){Hide(interactables[12]);Notice("A green bottle shard, worn smooth.",4);}break;
            }
        }
        void Light(){
            if(!lightDirty)return;lightDirty=false;
            rig.Begin();
            var harbor=new Dictionary<Cell,bool>{{CellAt(HarborMirror),journey.HarborSlash}};
            var harborEmitter=CellAt(FindEntityByName("HarborEmitter"));
            rig.Draw(Optics.Trace(harborEmitter,new Cell(1,0),CellAt(FindEntityByName("HarborReceiver")),harbor,new HashSet<Cell>(),-22,-12,-14,-7),harborEmitter);
            if(journey.Has(Restoration.Mounted)){
                var grid=new Dictionary<Cell,bool>();for(int i=0;i<3;i++)grid.Add(CellAt(mirrors[i]),(journey.Mirrors&(1<<i))!=0);
                var path=Optics.Trace(CellAt(Socket),new Cell(1,0),CellAt(Receiver),grid,stone,-5,10,7,16);
                var service=Optics.Trace(CellAt(Socket),new Cell(1,0),CellAt(ServiceReceiver),grid,stone,-5,10,7,16);
                if(service.Reached && Restore(Restoration.CourtUnlocked))Notice("Service door unlatched. The last mirror is reachable.",5);
                rig.Draw(service.Reached?service:path,CellAt(Socket));
                if(path.Reached && journey.Has(Restoration.CourtUnlocked) && Restore(Restoration.Relay))Notice("Stair receiver lit. The beacon can start.",5);
            }
            rig.Finish();
        }
        void OnUpdate(float dt){
            keys.Poll();dt=Math.Min(.1f,Math.Max(0,dt));
            if(!ready){ready=true;Light();}
            if(keys.Escape){if(pages!=null)CloseDialogue();else{paused=!paused;context.Text="";dialogue.Text=paused?"PAUSED":"";footer.Text=paused?"Enter   Resume":"";Presentation.Dialogue(DialoguePanel,paused,paused);}}
            if(paused){body.LinearVelocity=Vector2.Zero;if(keys.Advance){paused=false;dialogue.Text="";footer.Text="";Presentation.HideUI(DialoguePanel);}if(keys.Menu)Scene.LoadScene("Scenes/MainMenu.hazel");return;}
            if(keys.Journal && (pages==null || journal)){if(pages!=null && journal)CloseDialogue();else{Talk(Story.Journal(journey));journal=true;}}
            if(pages!=null){body.LinearVelocity=Vector2.Zero;if(keys.Advance || keys.Interact){if(++page>=pages.Length)CloseDialogue();else SetDialogue();}return;}
            clock+=dt;grace=Math.Max(0,grace-dt);noticeTime=Math.Max(0,noticeTime-dt);
            float x=(Input.IsKeyDown(KeyCode.D)||Input.IsKeyDown(KeyCode.Right)?1:0)-(Input.IsKeyDown(KeyCode.A)||Input.IsKeyDown(KeyCode.Left)?1:0);
            float y=(Input.IsKeyDown(KeyCode.W)||Input.IsKeyDown(KeyCode.Up)?1:0)-(Input.IsKeyDown(KeyCode.S)||Input.IsKeyDown(KeyCode.Down)?1:0);
            var motion=new Vector2(x,y);float length=motion.Length();body.LinearVelocity=length>0?motion*(Speed/length):Vector2.Zero;
            bob+=dt*(length>0?12:3);coat.LocalTranslation=new Vector3(0,length>0?.025f*(float)Math.Sin(bob):0,.02f);
            var p=Player.Translation;
            // Solid boundaries carry most collision. A shoreline fall is safely recoverable.
            if(p.X<-23 || p.X>23 || p.Y<-17 || p.Y>17){Checkpoint(true);return;}
            var phase=Tide.Phase(clock);
            bool atWorks=p.X<-8 && p.Y>1;
            tideLabel.Text=atWorks?"TIDE   "+(phase==TidePhase.Clear?"CLEAR":phase==TidePhase.Warning?"SURGE SOON":"SURGE   HIGH GROUND")+"  "+Math.Ceiling(Tide.Remaining(clock)):"";
            tideLabel.Color=phase==TidePhase.Clear?new Vector4(.55f,.86f,.92f,1):phase==TidePhase.Warning?new Vector4(1,.78f,.4f,1):new Vector4(1,.4f,.35f,1);
            if(shownPhase!=(int)phase){if(atWorks && phase==TidePhase.Warning)Sound(SoundWarning);shownPhase=(int)phase;foreach(var wave in surges)if(wave!=null)wave.Scale=phase==TidePhase.Surge?new Vector3(1,1,1):new Vector3(.001f,.001f,1);}
            foreach(var board in warningBoards)if(board!=null)board.Scale=phase==TidePhase.Warning?new Vector3(1,.75f+.25f*(float)Math.Sin(clock*10),1):new Vector3(1,1,1);
            if(grace<=0 && Tide.Exposed(p.X,p.Y) && (phase==TidePhase.Surge || !journey.Has(Restoration.Pump))){Checkpoint(true);return;}
            place.Text=p.Y<-6?"HARBOR LANDING":p.X<-8?"TIDE WORKS":p.X>10 && p.Y>8?"LOWER LIGHTHOUSE":p.Y>7?"MIRROR COURT":"SALT GROVE";
            nearest=-1;float best=InteractionRadius*InteractionRadius;
            for(int i=0;i<interactables.Length;i++){var e=interactables[i];if(e==null || !e.IsValid || Prompt(i)=="")continue;var distance=Distance(Player,e);if(distance<best){best=distance;nearest=i;}}
            context.Text=saveFailed || noticeTime>0?notice:nearest<0?"E   Interact nearby     Q   Journal     Esc   Pause":Prompt(nearest);
            if(keys.Interact && nearest>=0)Interact(nearest);
            Light();
        }
    }
}
