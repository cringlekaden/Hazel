using System;
using Skybound;
class FlightTests {
    static void Check(bool value,string message) { if(!value)throw new Exception(message); }
    static Flight Simulate(int fps) {
        var game=new Flight();
        for(int frame=0;frame<fps*30;frame++)game.Advance(1f/fps,frame==0||frame%fps==0);
        return game;
    }
    static int Main() {
        try {
            var authored=new Flight(positions:new float[]{6,12,18,24},gaps:new float[]{.2f,.4f,-.2f,0});
            Check(authored.Gates[0].X==6&&authored.Gates[1].X==12&&authored.Gates[0].Gap==.2f,"Authored initial obstacle layout was ignored");
            var ready=new Flight();
            ready.Advance(100,false);
            Check(ready.Phase==Flight.State.Ready&&ready.Y==0&&ready.Gates[0].X==4,"Ready state moved or spawned an immediate obstacle");
            ready.Advance(.1f,true);
            Check(ready.Phase==Flight.State.Flying&&ready.Y>0&&ready.Velocity>0,"Flap did not start responsive flight");
            for(int i=0;i<400;i++)ready.Advance(1f/120,false);
            Check(ready.Phase==Flight.State.Dead,"Ground collision missing");
            int score=ready.Score;float y=ready.Y;
            ready.Advance(1,true);
            Check(ready.Phase==Flight.State.Dead&&ready.Score==score&&ready.Y==y,"Fatal input restarted or advanced dead flight");
            var a=Simulate(30);var b=Simulate(120);
            Check(a.Phase==b.Phase&&a.Score==b.Score&&Math.Abs(a.Y-b.Y)<.03f,"Gameplay depends on rendering rate");
            var longRun=new Flight();var identities=(Flight.Gate[])longRun.Gates.Clone();
            for(int i=0;i<120*1200;i++) {
                Flight.Gate next=null;
                foreach(var gate in longRun.Gates)if(gate.X+Flight.GateHalfWidth>=Flight.BirdX-Flight.BirdRadius&&(next==null||gate.X<next.X))next=gate;
                float target=next==null?0:next.Gap;
                longRun.Advance(1f/120,i==0||(longRun.Y<target-.55f&&longRun.Velocity<.2f));
                Check(longRun.Phase==Flight.State.Flying,"Reachable-gap bot died at step "+i);
                Check(longRun.Gates.Length==4,"Obstacle count grew");
                for(int j=0;j<4;j++) {
                    Check(Object.ReferenceEquals(identities[j],longRun.Gates[j]),"Retired obstacle was replaced instead of reused");
                    Check(Math.Abs(longRun.Gates[j].Gap)<=1,"Gap position escaped playable range");
                }
            }
            Check(longRun.Score>500&&longRun.Score<650,"Missing or duplicate obstacle scoring: "+longRun.Score);
            var restart=new Flight();Check(restart.Score==0&&restart.Phase==Flight.State.Ready,"New run inherited stale state");
            var portrait=new Flight(positions:new float[]{100,106,112,118});portrait.SetBounds(-12.4f,11.5f);portrait.Advance(.1f,true);
            for(int i=0;i<200;i++)portrait.Advance(1f/120,false);
            Check(portrait.Phase==Flight.State.Flying&&portrait.Y<-4,"Portrait flight still collides with the old authored floor");
            for(int i=0;i<250;i++)portrait.Advance(1f/120,false);
            Check(portrait.Phase==Flight.State.Dead&&portrait.Y-Flight.BirdRadius<portrait.Floor,"Viewport floor did not match collision bounds");
            LightkeeperTests.Run();
            Console.WriteLine("PASS: ready/input/death isolation, fixed-step rate equivalence, fair gaps, 20-minute bounded pool and once-only scoring ("+longRun.Score+")");return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
