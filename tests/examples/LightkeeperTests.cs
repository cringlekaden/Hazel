using System;
using System.Collections.Generic;
using LastLightkeeper;
internal static class LightkeeperTests {
    static void Check(bool value,string why){if(!value)throw new Exception("Lightkeeper: "+why);}
    public static void Run(){
        var j=new Journey();Check(!j.Restore(Restoration.Lens) && !j.Restore(Restoration.Beacon),"Skipped restoration prerequisites");
        foreach(var step in new[]{Restoration.Harbor,Restoration.Wrench,Restoration.Pump,Restoration.Lens,Restoration.Mounted,Restoration.CourtUnlocked,Restoration.Relay,Restoration.Beacon}){
            Check(j.Restore(step),"Valid restoration failed: "+step);Check(!j.Restore(step),"Repeated reward: "+step);
        }
        j.Mirrors=3;j.Checkpoint=2;j.HarborSlash=true;j.Restore(Restoration.Archive);
        var copy=Journey.Decode(j.Encode());Check(copy.Encode()==j.Encode(),"Payload roundtrip changed journey");
        foreach(var text in new[]{"LK2|0|0|0|0","LK1|512|0|0|0","LK1|8|0|0|0","LK1|0|8|0|0","LK1|0|0|2|0","garbled","LK1|0|0|0|2"}){
            bool rejected=false;try{Journey.Decode(text);}catch(InvalidOperationException){rejected=true;}Check(rejected,"Invalid/future payload accepted: "+text);
        }
        for(int flags=0;flags<=511;flags++){
            string old="LK1|"+flags+"|3|0|1";
            try{var migrated=Journey.Decode(old);Check(((int)migrated.Restored&511)==flags,"LK1 lost a repair or optional discovery");Check(migrated.CaveCheckpoint==0 && migrated.Shutters==0,"Migration invented cliff progress");Check(Journey.Decode(migrated.Encode()).Encode()==migrated.Encode(),"Migrated payload failed roundtrip");}catch(InvalidOperationException){}
        }
        var completed=Journey.Decode("LK1|127|3|2|1");Check(completed.ResumeScene=="Scenes/CliffCistern.hazel" && completed.Has(Restoration.CourtUnlocked),"Completed player was sent back to chapter one");
        Check(!completed.Restore(Restoration.Mara) && !completed.Restore(Restoration.Cistern),"Cliff prerequisite bypass");
        foreach(var stage in new[]{Restoration.Focus,Restoration.Drain,Restoration.Cistern,Restoration.CaveGate,Restoration.Rescue,Restoration.Mara,Restoration.Watch}){Check(completed.Restore(stage),"Cliff milestone failed");Check(!completed.Restore(stage),"Cliff reward repeated");Check(Journey.Decode(completed.Encode()).Encode()==completed.Encode(),"Cliff stage save mismatch");}
        completed.CaveCheckpoint=2;Check(Journey.Decode(completed.Encode()).CaveCheckpoint==2,"Cave checkpoint lost");
        var oldCliff=Journey.Decode("LK2|131071|7|2|1|26|2");
        Check(oldCliff.ResumeScene=="Scenes/CliffDawn.hazel" && oldCliff.Carriage==2 && oldCliff.ObservatoryCheckpoint==0 && oldCliff.Encode()=="LK2|131071|7|2|1|26|2","LK2 migration changed prior repairs or settings");
        Check(!oldCliff.Restore(Restoration.Crew),"Survey crew prerequisite bypass");
        foreach(var step in new[]{Restoration.Collimator,Restoration.Span,Restoration.Dome,Restoration.Crew,Restoration.Winch,Restoration.Observatory}){
            Check(oldCliff.Restore(step),"Observatory restoration failed");Check(!oldCliff.Restore(step),"Survey reward repeated");
            var migrated=Journey.Decode(oldCliff.Encode());Check(migrated.Encode()==oldCliff.Encode() && migrated.Has(Restoration.Archive|Restoration.Cache) && migrated.Shutters==26,"LK3 lost old optional discoveries or cliff settings");
            if(step==Restoration.Winch)Check(migrated.ResumeScene=="Scenes/Observatory.hazel","Resume skipped crew departure before chapter completion");
        }
        oldCliff.ObservatoryCheckpoint=2;oldCliff.Carriage=1;Check(Journey.Decode(oldCliff.Encode()).Encode()==oldCliff.Encode(),"Survey recovery or carriage setting lost");
        Check(oldCliff.ResumeScene=="Scenes/ObservatoryDawn.hazel","Survey completion resumes at wrong endpoint");
        foreach(var bad in new[]{"LK3|0|0|0|0|0|0|1|2","LK3|0|0|0|0|0|0|0|0","LK3|8388608|0|0|0|0|0|0|2","LK3|131071|0|0|0|0|0|0|3","LK4|131071|0|0|0|0|0|0|2"}){
            bool rejected=false;try{Journey.Decode(bad);}catch(InvalidOperationException){rejected=true;}Check(rejected,"Damaged/future survey data accepted");
        }
        for(int cradle=0;cradle<3;cradle++){
            var block=new HashSet<Cell>{new Cell(10,SurveyRules.CarriageRow(cradle))};var straight=new Dictionary<Cell,bool>();
            Check(!Optics.Trace(new Cell(-19,0),new Cell(1,0),new Cell(13,0),straight,block,-24,24,-12,12,24).Reached,"Old sleeve reached distant survey latch");
            Check(Optics.Trace(new Cell(-19,0),new Cell(1,0),new Cell(13,0),straight,block,-24,24,-12,12,40).Reached==(cradle!=1),"Long-focus span/carriage obstruction inconsistent");
            Check(Optics.Trace(new Cell(19,2),new Cell(-1,0),new Cell(-13,2),straight,block,-24,24,-12,12,40).Reached==(cradle!=2),"Opposed sightline/carriage obstruction inconsistent");
        }
        for(int slot=0;slot<3;slot++){var shutters=new Journey();for(int n=0;n<3;n++)shutters.MoveShutter(slot);Check(shutters.Shutters==0,"Shutter cycle corrupted another bank");}
        foreach(var bad in new[]{"LK3|0|0|0|0|0|0","LK2|131072|0|0|0|0|0","LK2|0|0|0|0|27|0","LK2|0|0|0|0|0|2","LK2|16384|0|0|0|0|0"}){bool rejected=false;try{Journey.Decode(bad);}catch(InvalidOperationException){rejected=true;}Check(rejected,"Future/corrupt cliff save accepted");}
        for(int notch=0;notch<3;notch++){
            var blocks=new HashSet<Cell>();LanternRules.ShutterBlocks(blocks,0,LanternRules.Gap(false,0,notch));
            Check(Optics.Trace(new Cell(-8,0),new Cell(1,0),new Cell(8,0),new Dictionary<Cell,bool>(),blocks,-22,22,-12,12).Reached==(notch==1),"Drain shutter aperture inconsistent");
            var fixedMirrors=new Dictionary<Cell,bool>{{new Cell(8,3),true},{new Cell(8,6),true}};
            Check(Optics.Trace(new Cell(-3,3),new Cell(1,0),new Cell(12,6),fixedMirrors,blocks,-22,22,-12,12).Reached==(notch==2),"Raised bridge route inconsistent");
        }
        for(int a=0;a<3;a++)for(int b=0;b<3;b++){
            var blocks=new HashSet<Cell>();LanternRules.ShutterBlocks(blocks,-6,LanternRules.Gap(true,1,a));LanternRules.ShutterBlocks(blocks,10,LanternRules.Gap(true,2,b));
            Check(Optics.Trace(new Cell(-12,6),new Cell(1,0),new Cell(-1,6),new Dictionary<Cell,bool>(),blocks,-22,22,-12,12).Reached==(a==2),"Gallery shutter doesn't require upper notch");
            var route=new Dictionary<Cell,bool>{{new Cell(8,3),false},{new Cell(8,-3),false}};
            Check(Optics.Trace(new Cell(0,3),new Cell(1,0),new Cell(13,-3),route,blocks,-22,22,-12,12).Reached==(b==2),"Rescue shutter/fixed mirrors inconsistent");
        }
        Check(LanternRules.InFocus(4,0,new Cell(1,0)) && !LanternRules.InFocus(-4,0,new Cell(1,0)) && !LanternRules.InFocus(0,4,new Cell(1,0)) && !LanternRules.InFocus(7,0,new Cell(1,0)),"Wisp focus counterplay cone/range broken");
        var positions=new[]{new Cell(1,10),new Cell(1,15),new Cell(7,15)};int solutions=0;
        for(int mask=0;mask<8;mask++){
            var mirrors=new Dictionary<Cell,bool>();for(int i=0;i<3;i++)mirrors[positions[i]]=(mask&(1<<i))!=0;
            var beam=Optics.Trace(new Cell(-4,10),new Cell(1,0),new Cell(7,11),mirrors,new HashSet<Cell>{new Cell(4,10)},-5,10,7,16);
            if(beam.Reached){solutions++;Check(mask==3,"Unexpected main court solution");Check(beam.Steps.Count>15,"Court path shortcut");}
        }
        Check(solutions==1,"Authored mirror court must have one solution");
        var solved=new Dictionary<Cell,bool>{{positions[0],true},{positions[1],true},{positions[2],false}};
        Check(!Optics.Trace(new Cell(-4,10),new Cell(1,0),new Cell(7,11),solved,new HashSet<Cell>{new Cell(4,15)},-5,10,7,16).Reached,"Light passed through stone");
        var cycle=new Dictionary<Cell,bool>{{new Cell(0,0),true},{new Cell(2,0),false},{new Cell(2,-2),true},{new Cell(0,-2),false}};
        var loop=Optics.Trace(new Cell(0,0),new Cell(1,0),new Cell(4,4),cycle,new HashSet<Cell>(),-5,5,-5,5);
        Check(loop.Loop && loop.Steps.Count<256,"Mirror cycle not bounded");
        Check(Tide.Phase(4.99f)==TidePhase.Clear && Tide.Phase(5)==TidePhase.Warning && Tide.Phase(7)==TidePhase.Surge && Tide.Phase(10)==TidePhase.Clear,"Tide phase boundaries");
        Check(Tide.Exposed(-16,8) && !Tide.Exposed(-12,8),"Eastern alcove must be safe");
        for(int i=0;i<1000;i++)Check(Tide.Remaining(i*.031f)>0 && Tide.Remaining(i*.031f)<=5,"Tide countdown invalid");
        Console.WriteLine("PASS: Lightkeeper restoration prerequisites/idempotence, versioned progress, exhaustive mirror solution/occlusion/cycle and tide warning/safe-alcove contracts");
    }
}
