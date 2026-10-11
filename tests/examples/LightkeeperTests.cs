using System;
using System.Collections.Generic;
using LastLightkeeper;
internal static class LightkeeperTests {
    static void Check(bool value,string why){if(!value)throw new Exception("Lightkeeper: "+why);}
    public static void Run(){
        var j=new Journey();Check(!j.Restore(Restoration.Lens) && !j.Restore(Restoration.Beacon),"Skipped restoration prerequisites");
        foreach(var step in new[]{Restoration.Harbor,Restoration.Wrench,Restoration.Pump,Restoration.Lens,Restoration.Mounted,Restoration.Relay,Restoration.Beacon}){
            Check(j.Restore(step),"Valid restoration failed: "+step);Check(!j.Restore(step),"Repeated reward: "+step);
        }
        j.Mirrors=3;j.Checkpoint=2;j.HarborSlash=true;j.Restore(Restoration.Archive);
        var copy=Journey.Decode(j.Encode());Check(copy.Encode()==j.Encode(),"Payload roundtrip changed journey");
        foreach(var text in new[]{"LK2|0|0|0|0","LK1|512|0|0|0","LK1|8|0|0|0","LK1|0|8|0|0","LK1|0|0|2|0","garbled","LK1|0|0|0|2"}){
            bool rejected=false;try{Journey.Decode(text);}catch(InvalidOperationException){rejected=true;}Check(rejected,"Invalid/future payload accepted: "+text);
        }
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
