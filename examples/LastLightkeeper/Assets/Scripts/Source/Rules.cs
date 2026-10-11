using System;
using System.Collections.Generic;
using System.Globalization;
namespace LastLightkeeper {
    [Flags] public enum Restoration { None=0, Harbor=1, Wrench=2, Pump=4, Lens=8, Mounted=16, Relay=32, Beacon=64, Archive=128, Cache=256, CourtUnlocked=512, Focus=1024, Drain=2048, Cistern=4096, CaveGate=8192, Rescue=16384, Mara=32768, Watch=65536 }
    // Game-owned payload; native SaveData separately versions its storage envelope.
    public sealed class Journey {
        public Restoration Restored;
        public int Mirrors=4, Checkpoint=0;
        public bool HarborSlash=false;
        public int Shutters=0, CaveCheckpoint=0;
        public int Shutter(int slot) => Shutters/(slot==0?1:slot==1?3:9)%3;
        public void MoveShutter(int slot) { if(slot<0 || slot>2)throw new ArgumentOutOfRangeException("slot");var factor=slot==0?1:slot==1?3:9;Shutters+=((Shutter(slot)+1)%3-Shutter(slot))*factor; }
        public string ResumeScene => !Has(Restoration.Beacon)?"Scenes/Breakwater.hazel":Has(Restoration.Watch)?"Scenes/CliffDawn.hazel":Has(Restoration.Cistern) && CaveCheckpoint>0?"Scenes/SeaCaves.hazel":"Scenes/CliffCistern.hazel";
        public bool Has(Restoration value) => (Restored & value)==value;
        public bool Restore(Restoration step) {
            if(step==Restoration.None || !Enum.IsDefined(typeof(Restoration),step))return false;
            Restoration need=Restoration.None;
            switch(step) {
                case Restoration.Wrench:need=Restoration.Harbor;break;
                case Restoration.Pump:need=Restoration.Harbor|Restoration.Wrench;break;
                case Restoration.Lens:need=Restoration.Pump;break;
                case Restoration.Mounted:need=Restoration.Lens;break;
                case Restoration.Relay:need=Restoration.Mounted|Restoration.CourtUnlocked;break;
                case Restoration.Beacon:need=Restoration.Relay;break;
                case Restoration.CourtUnlocked:need=Restoration.Mounted;break;
                case Restoration.Focus:need=Restoration.Beacon;break;
                case Restoration.Drain:need=Restoration.Focus;break;
                case Restoration.Cistern:need=Restoration.Drain;break;
                case Restoration.CaveGate:need=Restoration.Cistern;break;
                case Restoration.Rescue:need=Restoration.CaveGate;break;
                case Restoration.Mara:need=Restoration.Rescue;break;
                case Restoration.Watch:need=Restoration.Mara;break;
            }
            if(!Has(need) || Has(step))return false;
            Restored|=step;return true;
        }
        public string Encode() => "LK2|"+((int)Restored).ToString(CultureInfo.InvariantCulture)+"|"+Mirrors+"|"+Checkpoint+"|"+(HarborSlash?1:0)+"|"+Shutters+"|"+CaveCheckpoint;
        public static Journey Decode(string payload) {
            if(String.IsNullOrEmpty(payload))return new Journey();
            var f=payload.Split('|');bool legacy=f.Length==5 && f[0]=="LK1";
            int flags,mirrors,checkpoint,harbor,shutters=0,cave=0;
            if((!legacy && (f.Length!=7 || f[0]!="LK2")) || !Int32.TryParse(f[1],out flags) || !Int32.TryParse(f[2],out mirrors) || !Int32.TryParse(f[3],out checkpoint) || !Int32.TryParse(f[4],out harbor) || flags<0 || flags>(legacy?511:131071) || mirrors<0 || mirrors>7 || checkpoint<0 || checkpoint>2 || harbor<0 || harbor>1 || (!legacy && (!Int32.TryParse(f[5],out shutters) || !Int32.TryParse(f[6],out cave))) || shutters<0 || shutters>26 || cave<0 || cave>2)
                throw new InvalidOperationException("This save is damaged or from a newer version. It has been kept.");
            var j=new Journey{Restored=(Restoration)flags,Mirrors=mirrors,Checkpoint=checkpoint,HarborSlash=harbor==1,Shutters=shutters,CaveCheckpoint=cave};
            if(legacy && j.Has(Restoration.Relay))j.Restored|=Restoration.CourtUnlocked;
            var ordered=new[]{Restoration.Harbor,Restoration.Wrench,Restoration.Pump,Restoration.Lens,Restoration.Mounted,Restoration.CourtUnlocked,Restoration.Relay,Restoration.Beacon,Restoration.Focus,Restoration.Drain,Restoration.Cistern,Restoration.CaveGate,Restoration.Rescue,Restoration.Mara,Restoration.Watch};
            for(int i=1;i<ordered.Length;i++)if(j.Has(ordered[i]) && !j.Has(ordered[i-1]))throw new InvalidOperationException("Invalid repair order. Original save kept.");
            if((checkpoint>0 && !j.Has(Restoration.Harbor)) || (checkpoint>1 && !j.Has(Restoration.Pump)) || (cave>0 && !j.Has(Restoration.Cistern)) || (cave>1 && !j.Has(Restoration.CaveGate)) || (j.Has(Restoration.CourtUnlocked) && !j.Has(Restoration.Mounted)))throw new InvalidOperationException("Invalid checkpoint. Original save kept.");
            // LK1 court completions received the compatible service-door default above.
            return j;
        }
    }

    public struct Cell : IEquatable<Cell> {
        public int X,Y; public Cell(int x,int y){X=x;Y=y;}
        public bool Equals(Cell other)=>X==other.X && Y==other.Y;
        public override bool Equals(object other)=>other is Cell && Equals((Cell)other);
        public override int GetHashCode()=>X*397^Y;
    }
    public struct RayStep { public Cell Position,Direction; public RayStep(Cell p,Cell d){Position=p;Direction=d;} }
    public sealed class LightPath {
        public readonly List<RayStep> Steps=new List<RayStep>();
        public bool Reached,Loop,Obstructed;
        public Cell End,EndDirection;
    }
    public static class Optics {
        public static Cell Reflect(Cell direction,bool slash)=>slash?new Cell(direction.Y,direction.X):new Cell(-direction.Y,-direction.X);
        // Discrete optics, with authored occluders. Bound and visited states stop loops.
        public static LightPath Trace(Cell emitter,Cell direction,Cell receiver,IDictionary<Cell,bool> mirrors,ISet<Cell> blocked,int minX,int maxX,int minY,int maxY,int reach=256) {
            var result=new LightPath{End=emitter,EndDirection=direction};var seen=new HashSet<string>();var p=emitter;
            for(int i=0;i<Math.Min(256,reach);i++) {
                p=new Cell(p.X+direction.X,p.Y+direction.Y);
                result.End=p;result.EndDirection=direction;
                if(p.X<minX || p.X>maxX || p.Y<minY || p.Y>maxY)return result;
                if(blocked.Contains(p)){result.Obstructed=true;return result;}
                var key=p.X+","+p.Y+","+direction.X+","+direction.Y;
                if(!seen.Add(key)){result.Loop=true;return result;}
                result.Steps.Add(new RayStep(p,direction));
                if(p.Equals(receiver)){result.Reached=true;return result;}
                bool slash;if(mirrors.TryGetValue(p,out slash))direction=Reflect(direction,slash);
            }
            result.Loop=reach>=256;return result;
        }
    }
    public enum TidePhase { Clear, Warning, Surge }
    public static class Tide {
        public const float Period=10;
        public static TidePhase Phase(float seconds) {
            var t=((seconds%Period)+Period)%Period;
            return t<5?TidePhase.Clear:t<7?TidePhase.Warning:TidePhase.Surge;
        }
        public static float Remaining(float seconds) {
            var t=((seconds%Period)+Period)%Period;return t<5?5-t:t<7?7-t:10-t;
        }
        public static bool Exposed(float x,float y)=>x>=-18.35f && x<=-12.6f && y>=6.6f && y<=10.4f;
    }
}
namespace LastLightkeeper {
    public static class LanternRules {
        public static int Gap(bool caves,int slot,int state) {
            if(slot<0 || slot>2 || state<0 || state>2)throw new System.ArgumentOutOfRangeException();
            return !caves?new[]{-3,0,3}[state]:slot==1?new[]{0,3,6}[state]:new[]{0,3,-3}[state];
        }
        public static void ShutterBlocks(System.Collections.Generic.ISet<Cell> blocks,int x,int gap,int minY=-8,int maxY=8) {
            for(int y=minY;y<=maxY;y++)if(y!=gap)blocks.Add(new Cell(x,y));
        }
        public static bool InFocus(float dx,float dy,Cell facing) {
            float distance=dx*dx+dy*dy;
            return distance>.01f && distance<=36 && (dx*facing.X+dy*facing.Y)>System.Math.Sqrt(distance)*.8;
        }
    }
}
