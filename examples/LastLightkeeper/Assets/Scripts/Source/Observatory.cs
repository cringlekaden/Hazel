using System;
using System.Collections.Generic;
using Hazel;
namespace LastLightkeeper {
    // Authored survey posts and moving equipment; no generated rooms or engine puzzle system.
    public class Observatory : Entity {
        public Entity Player,Objective,Context,Place,Inventory,TideLabel,Dialogue,DialoguePanel,SaveLabel;
        public Entity SoundTurn,SoundRestore,SoundHazard,SoundWarning;
        public Prefab BeamAsset,EndAsset;
        public float Speed=3.25f;
        Journey journey; Controls keys; Rigidbody2DComponent body; LightRig rig;
        TextComponent objective,context,dialogue,footer,saveLabel;
        string[] pages; int page; Action after;
        bool paused,journal,saveBlocked,saveFailed; string notice=""; float noticeTime,bob;
        Cell facing=new Cell(1,0);
        readonly HashSet<Cell> walls=new HashSet<Cell>();
        readonly Dictionary<Cell,bool> noMirrors=new Dictionary<Cell,bool>();

        void OnCreate(){
            keys=new Controls();body=Player.GetComponent<Rigidbody2DComponent>();
            objective=Objective.GetComponent<TextComponent>();context=Context.GetComponent<TextComponent>();
            dialogue=Dialogue.GetComponent<TextComponent>();footer=FindEntityByName("DialogueFooter").GetComponent<TextComponent>();saveLabel=SaveLabel.GetComponent<TextComponent>();
            try{journey=Journey.Decode(SaveData.Read("journey"));}catch(Exception ex){journey=new Journey();saveBlocked=true;notice="Save kept. Return to the title to choose a new game.";noticeTime=20;Console.WriteLine(ex.Message);}
            if(!SaveData.IsPersistent && !journey.Has(Restoration.Watch))journey=Journey.Decode("LK2|130687|3|2|1|26|2");
            rig=new LightRig(BeamAsset,EndAsset);
            for(int i=0;;i++){var e=FindEntityByName("OpticalStone"+i);if(e==null)break;walls.Add(CellAt(e));}
            Apply();Recover(false);ShowDialogue();
            Place.GetComponent<TextComponent>().Text=journey.ObservatoryCheckpoint==2?"OBSERVATORY - INSTRUMENT ROOM":journey.ObservatoryCheckpoint==1?"OBSERVATORY - EAST TERRACE":"OBSERVATORY - WEST TERRACE";
            saveLabel.Text=SaveData.IsPersistent?"Autosave on":"Editor practice - resets after Stop";
            if(!journey.Has(Restoration.Collimator))Talk(new[]{"IONA\nThe roof came through the survey floor.\nTheir long-focus lens is still on the repair bench.\nMy sleeve will take the same fitting."});
        }
        void OnDestroy(){rig?.Destroy();}
        static Cell CellAt(Entity e){var p=e.Translation;return new Cell((int)Math.Round(p.X),(int)Math.Round(p.Y));}
        static void Hide(Entity e){if(e!=null && e.IsValid)e.Scale=new Vector3(.001f,.001f,1);}
        bool Near(string name,float radius=1.65f){var e=FindEntityByName(name);if(e==null || !e.IsValid)return false;var a=Player.Translation;var b=e.Translation;return (a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)<radius*radius;}
        void Notice(string text,float seconds=4){if(saveFailed)return;notice=text;noticeTime=seconds;}
        void Sound(Entity e){try{e?.GetComponent<AudioSourceComponent>().Play();}catch(Exception ex){Console.WriteLine("Lightkeeper audio: "+ex.Message);}}
        bool Persist(){
            if(saveBlocked)return false;
            try{SaveData.Write("journey",journey.Encode());saveFailed=false;saveLabel.Text=SaveData.IsPersistent?"Progress saved":"Editor practice";return true;}
            catch(Exception ex){saveLabel.Text="Not saved - retry before quitting";notice="Couldn't save. Retry at a survey post before quitting.";noticeTime=10;saveFailed=true;Console.WriteLine(ex.Message);return false;}
        }
        bool Restore(Restoration step){if(saveBlocked || !journey.Restore(step))return false;Apply();Sound(SoundRestore);Persist();return true;}
        void Remove(string name){var e=FindEntityByName(name);if(e!=null && e.IsValid)e.Destroy();}
        void Apply(){
            if(journey.Has(Restoration.Collimator))Hide(FindEntityByName("Collimator"));
            foreach(var latch in new[]{Restoration.Span,Restoration.Dome,Restoration.Winch}){
                string receiver=latch==Restoration.Span?"SpanReceiver":latch==Restoration.Dome?"DomeReceiver":"TowerReceiver";
                if(journey.Has(latch))FindEntityByName(receiver).GetComponent<SpriteRendererComponent>().SetSprite(Art.Region("tiny-factory",99));
            }
            if(journey.Has(Restoration.Span)){
                Remove("SpanGate");
                FindEntityByName("BridgeDeck").Scale=new Vector3(1,1,1);
            }
            if(journey.Has(Restoration.Dome))Remove("CrewGate");
            if(journey.Has(Restoration.Winch))Remove("TowerGate");
            var carriage=FindEntityByName("SurveyCarriage");carriage.Translation=new Vector3(10,SurveyRules.CarriageRow(journey.Carriage),.45f);
            objective.Text=journey.Has(Restoration.Collimator)?Story.Objective(journey):"Fit the long-focus lens at the western repair bench";
            Inventory.GetComponent<TextComponent>().Text=journey.Has(Restoration.Collimator)?"LONG-FOCUS LANTERN    Space: focus    Aim with movement":"LANTERN    Fit the survey lens at the west bench";
        }
        void Recover(bool hit){
            Player.Translation=FindEntityByName("Checkpoint"+journey.ObservatoryCheckpoint).Translation;body.LinearVelocity=Vector2.Zero;
            if(hit){Sound(SoundHazard);Notice("Back on the survey terrace. Repairs kept.");}
        }
        void Talk(string[] text,Action done=null){pages=text;page=0;after=done;journal=false;context.Text="";body.LinearVelocity=Vector2.Zero;ShowDialogue();}
        void ShowDialogue(){dialogue.Text=pages==null?"":pages[page];footer.Text=pages==null?"":"Enter   "+(page+1==pages.Length?(after!=null?"Continue":"Close"):"Next    "+(page+1)+" of "+pages.Length);Presentation.Dialogue(DialoguePanel,pages!=null,false);}
        void CloseDialogue(){pages=null;ShowDialogue();var callback=after;after=null;callback?.Invoke();}
        void Focus(bool active){
            rig.Begin();
            if(active && journey.Has(Restoration.Focus)){
                var emitter=CellAt(Player);var blocked=new HashSet<Cell>(walls){CellAt(FindEntityByName("SurveyCarriage"))};
                var target=journey.Has(Restoration.Crew)?new Cell(13,7):journey.Has(Restoration.Span)?new Cell(-13,2):new Cell(13,0);
                var path=Optics.Trace(emitter,facing,target,noMirrors,blocked,-24,24,-12,12,journey.Has(Restoration.Collimator)?40:24);
                rig.Draw(path,emitter);
                if(path.Reached){
                    if(!journey.Has(Restoration.Span)){if(Restore(Restoration.Span))Notice("Span latch caught. Cross the southern bridge.",6);}
                    else if(!journey.Has(Restoration.Dome)){if(Restore(Restoration.Dome))Notice("North door released. The crew can hear you.",6);}
                    else if(journey.Has(Restoration.Crew) && Restore(Restoration.Winch))Notice("Tower winch powered. Meet the crew at the west stairs.",6);
                }
            }
            rig.Finish();
        }
        string Prompt(){
            if(Near("Collimator") && !journey.Has(Restoration.Collimator))return "E   Fit the survey lens";
            if(Near("CarriageHandle"))return "E   Winch the survey carriage to the next cradle";
            if(Near("Nessa",2))return "E   Talk to Nessa";
            if(Near("Cal",2))return "E   Talk to Cal";
            if(Near("SurveyNote"))return "E   Read the survey maintenance card";
            if(Near("WestPost"))return "Face right; focus across the broken floor";
            if(Near("EastPost"))return "Face left; focus toward the west door latch";
            if(Near("NorthPost"))return "Face right; focus toward the tower winch";
            if(Near("TowerExit"))return journey.Has(Restoration.Winch)?"E   Meet the crew at the tower stairs":"Tower winch has no power";
            if(Near("RestMarker0") || Near("RestMarker1") || Near("RestMarker2"))return "E   Save at the survey post";
            return "E   Nearby equipment    Q   Notes    Esc   Pause";
        }
        void Interact(){
            if(Near("Collimator") && !journey.Has(Restoration.Collimator)){
                if(Restore(Restoration.Collimator))Talk(new[]{"LONG-FOCUS LENS\nThe lantern now reaches across the survey hall.\nHold Space as before. The white posts mark the\nold sightlines; clear equipment out of their way."});return;
            }
            if(Near("CarriageHandle") && journey.Has(Restoration.Span)){
                journey.Carriage=(journey.Carriage+1)%3;Apply();Persist();Sound(SoundTurn);
                Notice("Carriage in the "+(journey.Carriage==0?"lower":journey.Carriage==1?"middle":"upper")+" cradle.",2);return;
            }
            if(Near("Nessa",2) && journey.Has(Restoration.Dome)){
                if(Restore(Restoration.Crew))Talk(new[]{"NESSA\nI went back up for Cal. Then the floor gave way.\nDid Orrin get the others off the ledge?","IONA\nThey're aboard. Mara too.\nShe sent the ledger. Can you read the tower entry?","NESSA\nCal copied it before the roof fell.\nWe'll take the winch brake. You light the drive\nfrom the north post; then we'll get you up."});
                else Talk(new[]{"NESSA\nThe brake's holding. Cal's got the settings.\nI'd like to stop watching my brother fight that reef."});return;
            }
            if(Near("Cal",2) && journey.Has(Restoration.Dome)){
                Talk(new[]{journey.Has(Restoration.Crew)?"CAL\nTower entry: keep the lamp on the drive receiver.\nNorth post, facing east. No mirror in that line.\nI checked it twice this morning.":"CAL\nNessa's at the window. Tell her about the boat.\nI've got your father's tower settings here."});return;
            }
            if(Near("SurveyNote")){Talk(new[]{"SURVEY MAINTENANCE\nWest terrace: span motor. East terrace: north door.\nPark the carriage clear of the line before testing.\nMotor latches hold without the lamp."});return;}
            if(Near("TowerExit") && journey.Has(Restoration.Winch)){
                Talk(new[]{"CAL\nI'll take the spare lamp down to the landing.\nOrrin can follow its marks on the return trip.","NESSA\nI'll hold the brake until you're through.\nTell my brother I haven't lost his good compass.","IONA\nHe'll ask about it before he asks about you.\nThank you. I'll take the tower from here."},()=>{
                    // A failed departure write leaves the active chapter resumable.
                    var before=journey.Restored;journey.Restore(Restoration.Observatory);
                    if(Persist())Scene.LoadScene("Scenes/ObservatoryDawn.hazel");else journey.Restored=before;
                });return;
            }
            if(Near("RestMarker0") || Near("RestMarker1") || Near("RestMarker2")){if(Persist())Notice("Progress saved at the survey post.");}
        }
        void OnUpdate(float dt){
            keys.Poll();dt=Math.Min(.1f,Math.Max(0,dt));
            if(keys.Escape){if(pages!=null)CloseDialogue();else{paused=!paused;context.Text="";dialogue.Text=paused?"PAUSED":"";footer.Text=paused?"Enter   Resume":"";Presentation.Dialogue(DialoguePanel,paused,paused);}}
            if(paused){body.LinearVelocity=Vector2.Zero;if(keys.Menu && (saveBlocked || Persist()))Scene.LoadScene("Scenes/MainMenu.hazel");if(keys.Advance){paused=false;ShowDialogue();}return;}
            if(keys.Journal && journal && pages!=null)CloseDialogue();
            else if(keys.Journal && pages==null){Talk(new[]{"OBSERVATORY\n"+Story.Objective(journey)+".\nWASD or arrows: move. E: use. Hold Space: focus.\nWhite survey posts face their distant receivers.","THE BROKEN HALL\nSouthern bridge joins the terraces.\nEast winch moves the carriage, not the receivers.\nThe crew are behind the north instrument door."});journal=true;}
            if(pages!=null){body.LinearVelocity=Vector2.Zero;if(keys.Advance || keys.Interact){if(++page>=pages.Length)CloseDialogue();else ShowDialogue();}return;}
            noticeTime=Math.Max(0,noticeTime-dt);bob+=dt;
            float x=(Input.IsKeyDown(KeyCode.D)||Input.IsKeyDown(KeyCode.Right)?1:0)-(Input.IsKeyDown(KeyCode.A)||Input.IsKeyDown(KeyCode.Left)?1:0);
            float y=(Input.IsKeyDown(KeyCode.W)||Input.IsKeyDown(KeyCode.Up)?1:0)-(Input.IsKeyDown(KeyCode.S)||Input.IsKeyDown(KeyCode.Down)?1:0);
            if(x!=0)facing=new Cell(x>0?1:-1,0);else if(y!=0)facing=new Cell(0,y>0?1:-1);
            bool focus=Input.IsKeyDown(KeyCode.Space);var motion=new Vector2(x,y);float length=motion.Length();
            body.LinearVelocity=length>0?motion*(Speed*(focus?.5f:1)/length):Vector2.Zero;
            FindEntityByName("IonaCoat").LocalTranslation=new Vector3(0,length>0?.025f*(float)Math.Sin(bob*12):0,.02f);
            var p=Player.Translation;
            if(p.X<-23 || p.X>23 || p.Y<-10 || p.Y>10){Recover(true);return;}
            int checkpoint=journey.Has(Restoration.Dome) && p.Y>4?2:journey.Has(Restoration.Span) && p.X>15?1:0;
            if(checkpoint>journey.ObservatoryCheckpoint){journey.ObservatoryCheckpoint=checkpoint;if(Persist())Notice("Survey terrace reached. Checkpoint saved.");}
            if(keys.Interact)Interact();if(pages==null)Focus(focus);
            Place.GetComponent<TextComponent>().Text=p.Y>4?"OBSERVATORY - INSTRUMENT ROOM":p.X>0?"OBSERVATORY - EAST TERRACE":"OBSERVATORY - WEST TERRACE";
            TideLabel.GetComponent<TextComponent>().Text="";context.Text=saveFailed?notice:pages!=null?"":noticeTime>0?notice:Prompt();
        }
    }
}
