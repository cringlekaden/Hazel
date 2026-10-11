using System;
using System.Collections.Generic;
using Hazel;
namespace LastLightkeeper {
    public class Breakwater : Entity {
        public Entity Player,Camera,HarborMirror,HarborGate,Pump,Lens,Socket,Receiver,Tower,Wrench;
        public Entity MirrorOne,MirrorTwo,MirrorThree;
        public Entity Objective,Context,Place,Inventory,TideLabel,Dialogue,DialoguePanel,SaveLabel;
        public Entity SoundTurn,SoundRestore,SoundHazard;
        public Prefab BeamAsset;
        public float Speed=3.25f,InteractionRadius=1.5f;
        private Journey journey;
        private Controls keys;
        private Rigidbody2DComponent body;
        private Entity[] interactables,mirrors,beam,surges;
        private string[] pages;
        private int page,nearest=-1;
        private Action afterDialogue;
        private bool ready,paused,journal,saveBlocked,lightDirty=true;
        private int shownPhase=-1;
        private float clock,grace,noticeTime,bob;
        private Entity coat;
        private TextComponent objective,context,place,inventory,tideLabel,dialogue,saveLabel;
        private string notice="";
        private readonly HashSet<Cell> stone=new HashSet<Cell>();
        void OnCreate(){
            keys=new Controls();body=Player.GetComponent<Rigidbody2DComponent>();
            objective=Objective.GetComponent<TextComponent>();context=Context.GetComponent<TextComponent>();place=Place.GetComponent<TextComponent>();
            inventory=Inventory.GetComponent<TextComponent>();tideLabel=TideLabel.GetComponent<TextComponent>();dialogue=Dialogue.GetComponent<TextComponent>();saveLabel=SaveLabel.GetComponent<TextComponent>();
            try{journey=Journey.Decode(SaveData.Read("journey"));}catch(Exception ex){journey=new Journey();saveBlocked=true;Notice("Save kept / return to title to resolve",12);Console.WriteLine("Lightkeeper save: "+ex.Message);}
            mirrors=new[]{MirrorOne,MirrorTwo,MirrorThree};
            interactables=new[]{HarborMirror,FindEntityByName("Orrin"),Wrench,FindEntityByName("MaraNote"),Pump,Lens,Socket,MirrorOne,MirrorTwo,MirrorThree,Tower,FindEntityByName("Archive"),FindEntityByName("Cache")};
            coat=FindEntityByName("IonaCoat");
            saveLabel.Text=SaveData.IsPersistent?"Journey resumed":"Practice journey / progress resets after Play";
            for(int i=0;i<5;i++){var e=FindEntityByName("Occluder"+i);if(e!=null)stone.Add(CellAt(e));}
            surges=new Entity[20];for(int i=0;i<20;i++)surges[i]=FindEntityByName("Surge"+i);
            beam=new Entity[12];
            // One authored textured prefab; never create terrain or rooms at runtime.
            for(int i=0;i<beam.Length;i++){beam[i]=Entity.Instantiate(BeamAsset,new Vector3(0,0,-9));beam[i].Scale=new Vector3(.001f,.001f,1);}
            ApplyJourney();Checkpoint(false);SetDialogue();
            if(journey.Restored==Restoration.None)Talk(Story.Arrival);
            else Notice("Journey resumed / "+Story.Objective(journey),4);
        }
        void OnDestroy(){if(beam!=null)foreach(var e in beam)if(e!=null && e.IsValid)e.Destroy();}
        static Cell CellAt(Entity e){var p=e.Translation;return new Cell((int)Math.Round(p.X),(int)Math.Round(p.Y));}
        static float Distance(Entity a,Entity b){var p=a.Translation;var q=b.Translation;return (p.X-q.X)*(p.X-q.X)+(p.Y-q.Y)*(p.Y-q.Y);}
        static void Hide(Entity e){if(e!=null && e.IsValid)e.Scale=new Vector3(.001f,.001f,1);}
        void Notice(string text,float duration=3){notice=text;noticeTime=duration;}
        void Sound(Entity e){try{if(e!=null)e.GetComponent<AudioSourceComponent>().Play();}catch(Exception ex){Console.WriteLine("Lightkeeper audio: "+ex.Message);}}
        void Persist(){
            if(saveBlocked)return;
            try{SaveData.Write("journey",journey.Encode());saveLabel.Text=SaveData.IsPersistent?"Journey saved":"Editor Play / temporary journey";}
            catch(Exception ex){saveLabel.Text="SAVE FAILED / progress held in this session";Notice("Cannot save. Keep this session open; retry at a checkpoint.",8);Console.WriteLine("Lightkeeper save: "+ex.Message);}
        }
        bool Restore(Restoration step){if(!journey.Restore(step))return false;Sound(SoundRestore);ApplyJourney();Persist();return true;}
        void ApplyJourney(){
            lightDirty=true;
            HarborMirror.LocalRotation=new Vector3(0,0,journey.HarborSlash?0:(float)Math.PI/2);
            for(int i=0;i<3;i++)mirrors[i].LocalRotation=new Vector3(0,0,(journey.Mirrors&(1<<i))!=0?0:(float)Math.PI/2);
            if(journey.Has(Restoration.Harbor) && HarborGate!=null && HarborGate.IsValid)HarborGate.Destroy();
            if(journey.Has(Restoration.Wrench))Hide(Wrench);
            if(journey.Has(Restoration.Lens))Hide(Lens);
            if(journey.Has(Restoration.Pump))Pump.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Mounted))Socket.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",87));
            if(journey.Has(Restoration.Relay))Receiver.GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Harbor))FindEntityByName("HarborReceiver").GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",99));
            if(journey.Has(Restoration.Cache))Hide(interactables[12]);
            if(journey.Has(Restoration.Beacon)){FindEntityByName("BeaconHalo").Scale=new Vector3(2.4f,2.4f,1);FindEntityByName("Lantern mechanism").GetComponent<SpriteRendererComponent>().SetSprite(Region("tiny-factory",87));}
            objective.Text=Story.Objective(journey);
            inventory.Text=(journey.Has(Restoration.Wrench)?"WRENCH  ":"")+(journey.Has(Restoration.Lens)?"LENS  ":"")+(journey.Has(Restoration.Cache)?"SEA GLASS / found":"");
        }
        // Stable IDs are loaded from the shipped sheet identity table, not regenerated.
        static Sprite Region(string pack,int tile)=>Art.Region(pack,tile);
        void Talk(string[] text,Action done=null){pages=text;page=0;afterDialogue=done;journal=false;SetDialogue();}
        void SetDialogue(){dialogue.Text=pages==null?"":pages[page]+"\n\nENTER / SPACE / E    "+(page+1)+" / "+pages.Length;DialoguePanel.Scale=pages==null?new Vector3(.001f,.001f,1):new Vector3(27,5.3f,1);}
        void CloseDialogue(){pages=null;SetDialogue();var callback=afterDialogue;afterDialogue=null;callback?.Invoke();}
        void Checkpoint(bool announce){
            var marker=FindEntityByName("Checkpoint"+journey.Checkpoint);Player.Translation=marker.Translation;body.LinearVelocity=Vector2.Zero;grace=.65f;
            if(announce){Notice("The wave passes. Your repairs are safe.",4);Sound(SoundHazard);}
        }
        string Prompt(int i){
            switch(i){case 0:return journey.Has(Restoration.Harbor)?"Harbor signal / restored":"E / Turn the harbor mirror";
                case 1:return "E / Talk to Orrin";case 2:return journey.Has(Restoration.Wrench)?"":"E / Recover Mara's wrench";
                case 3:return "E / Read Mara's field note";case 4:return journey.Has(Restoration.Pump)?"E / Read the tide log":"E / Repair the sluice";
                case 5:return journey.Has(Restoration.Lens)?"":"E / Recover the lighthouse lens";
                case 6:return journey.Has(Restoration.Mounted)?"E / Read relay instructions":"E / Fit the lens";
                case 7:case 8:case 9:return "E / Turn mirror "+(i-6);
                case 10:return "E / Climb to the lower lantern";case 11:return "E / Read the keeper's archive";case 12:return journey.Has(Restoration.Cache)?"":"E / Take a piece of sea glass";default:return "";}
        }
        void Interact(int i){
            switch(i){
                case 0:
                    if(journey.Has(Restoration.Harbor)){Notice("The harbor light is steady.");break;}
                    journey.HarborSlash=!journey.HarborSlash;Sound(SoundTurn);ApplyJourney();
                    if(journey.HarborSlash){Restore(Restoration.Harbor);journey.Checkpoint=1;Persist();Notice("Harbor restored / the bridge is open",5);}
                    else Persist();break;
                case 1:Talk(Story.Ferryman);break;
                case 2:if(!journey.Has(Restoration.Harbor)){Notice("Open the harbor bridge first.");break;}if(Restore(Restoration.Wrench))Talk(Story.Grove);break;
                case 3:Talk(Story.Grove);break;
                case 4:
                    if(journey.Has(Restoration.Pump)){Talk(Story.Pump);Persist();break;}
                    if(!journey.Has(Restoration.Wrench)){Notice("The wheel is seized. Mara's wrench is in the eastern grove.",5);break;}
                    Restore(Restoration.Pump);journey.Checkpoint=2;Persist();Talk(Story.Pump);break;
                case 5:
                    if(!journey.Has(Restoration.Pump)){Notice("The drain must be repaired before the lens can be freed.",5);break;}
                    if(Restore(Restoration.Lens))Talk(Story.Lens);break;
                case 6:
                    if(!journey.Has(Restoration.Lens)){Talk(Story.Relay);break;}
                    if(Restore(Restoration.Mounted))Notice("The court wakes / follow the amber light",5);else Talk(Story.Relay);break;
                case 7:case 8:case 9:
                    if(!journey.Has(Restoration.Mounted)){Notice("Recover the lens and fit it in the western socket.",5);break;}
                    journey.Mirrors^=1<<(i-7);Sound(SoundTurn);ApplyJourney();Persist();break;
                case 10:
                    if(!journey.Has(Restoration.Relay)){Notice("The lantern needs power from the mirror court.",5);break;}
                    Restore(Restoration.Beacon);Talk(Story.Ending,()=>Scene.LoadScene("Scenes/Dawn.hazel"));break;
                case 11:Restore(Restoration.Archive);Talk(new[]{"KEEPER'S ARCHIVE / A letter never sent\nMara, the table is still set. If the sea takes the path,\nI will leave the light on. Come home when you can.","IONA\nHe never sent it. He thought there would be time.\nI will not make the same mistake."});break;
                case 12:if(Restore(Restoration.Cache)){Hide(interactables[12]);Notice("Sea glass / a small piece of the storm to keep",4);}break;
            }
        }
        int DrawPath(LightPath path,int index){
            // Combine each straight run; reflection corners remain under their authored mirror faces.
            for(int begin=0;begin<path.Steps.Count;){
                int end=begin;var direction=path.Steps[begin].Direction;
                while(end+1<path.Steps.Count && path.Steps[end+1].Direction.Equals(direction))end++;
                var first=begin==0?path.Steps[begin].Position:path.Steps[begin-1].Position;
                var last=path.Steps[end].Position;
                if(index>=beam.Length)break;
                var e=beam[index++];e.Translation=new Vector3((first.X+last.X)*.5f,(first.Y+last.Y)*.5f,.12f);
                float length=Math.Abs(last.X-first.X)+Math.Abs(last.Y-first.Y)+1;
                e.Scale=direction.X!=0?new Vector3(length,.13f,1):new Vector3(.13f,length,1);
                begin=end+1;
            }
            return index;
        }
        void Light(){
            if(!lightDirty)return;lightDirty=false;
            int drawn=0;
            var harbor=new Dictionary<Cell,bool>{{CellAt(HarborMirror),journey.HarborSlash}};
            drawn=DrawPath(Optics.Trace(CellAt(FindEntityByName("HarborEmitter")),new Cell(1,0),CellAt(FindEntityByName("HarborReceiver")),harbor,new HashSet<Cell>(),-22,-12,-14,-7),drawn);
            if(journey.Has(Restoration.Mounted)){
                var grid=new Dictionary<Cell,bool>();for(int i=0;i<3;i++)grid.Add(CellAt(mirrors[i]),(journey.Mirrors&(1<<i))!=0);
                var path=Optics.Trace(CellAt(Socket),new Cell(1,0),CellAt(Receiver),grid,stone,-5,10,7,16);
                drawn=DrawPath(path,drawn);
                if(path.Reached && Restore(Restoration.Relay))Notice("Relay restored / the lower beacon is ready",5);
            }
            for(int i=drawn;i<beam.Length;i++)Hide(beam[i]);
        }
        void OnUpdate(float dt){
            keys.Poll();dt=Math.Min(.1f,Math.Max(0,dt));
            if(!ready){ready=true;Light();}
            if(keys.Escape){if(pages!=null)CloseDialogue();else{paused=!paused;dialogue.Text=paused?"THE ISLAND CAN WAIT\n\nENTER / Resume     M / Title & saved journey":"";DialoguePanel.Scale=paused?new Vector3(27,5.3f,1):new Vector3(.001f,.001f,1);}}
            if(paused){body.LinearVelocity=Vector2.Zero;if(keys.Advance){paused=false;dialogue.Text="";Hide(DialoguePanel);}if(keys.Menu)Scene.LoadScene("Scenes/MainMenu.hazel");return;}
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
            tideLabel.Text=atWorks?"TIDE / "+(phase==TidePhase.Clear?"CLEAR":phase==TidePhase.Warning?"SURGE SOON":"SURGE / HIGH GROUND")+"  "+Math.Ceiling(Tide.Remaining(clock)):"";
            tideLabel.Color=phase==TidePhase.Clear?new Vector4(.55f,.86f,.92f,1):phase==TidePhase.Warning?new Vector4(1,.78f,.4f,1):new Vector4(1,.4f,.35f,1);
            if(shownPhase!=(int)phase){shownPhase=(int)phase;foreach(var wave in surges)if(wave!=null)wave.Scale=phase==TidePhase.Surge?new Vector3(1,1,1):new Vector3(.001f,.001f,1);}
            if(grace<=0 && Tide.Exposed(p.X,p.Y) && (phase==TidePhase.Surge || !journey.Has(Restoration.Pump))){Checkpoint(true);return;}
            place.Text=p.Y<-6?"THE BREAKWATER / Harbor landing":p.X<-8?"THE BREAKWATER / Tide works":p.X>10 && p.Y>8?"THE BREAKWATER / Lower lighthouse":p.Y>7?"THE BREAKWATER / Mirror court":"THE BREAKWATER / Salt grove";
            nearest=-1;float best=InteractionRadius*InteractionRadius;
            for(int i=0;i<interactables.Length;i++){var e=interactables[i];if(e==null || !e.IsValid || Prompt(i)=="")continue;var distance=Distance(Player,e);if(distance<best){best=distance;nearest=i;}}
            context.Text=noticeTime>0?notice:nearest<0?"E / Interact nearby     Q / Journal     ESC / Pause":Prompt(nearest);
            if(keys.Interact && nearest>=0)Interact(nearest);
            Light();
        }
    }
}
