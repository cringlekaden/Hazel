using System;
using System.Collections.Generic;
using System.Globalization;
namespace LastLightkeeper {
    [Flags] public enum Restoration { None=0, Harbor=1, Wrench=2, Pump=4, Lens=8, Mounted=16, Relay=32, Beacon=64, Archive=128, Cache=256 }
    // Game-owned payload; native SaveData separately versions its storage envelope.
    public sealed class Journey {
        public Restoration Restored;
        public int Mirrors=4, Checkpoint=0;
        public bool HarborSlash=false;
        public bool Has(Restoration value) => (Restored & value)==value;
        public bool Restore(Restoration step) {
            if(step==Restoration.None || !Enum.IsDefined(typeof(Restoration),step))return false;
            Restoration need=Restoration.None;
            switch(step) {
                case Restoration.Wrench:need=Restoration.Harbor;break;
                case Restoration.Pump:need=Restoration.Harbor|Restoration.Wrench;break;
                case Restoration.Lens:need=Restoration.Pump;break;
                case Restoration.Mounted:need=Restoration.Lens;break;
                case Restoration.Relay:need=Restoration.Mounted;break;
                case Restoration.Beacon:need=Restoration.Relay;break;
            }
            if(!Has(need) || Has(step))return false;
            Restored|=step;return true;
        }
        public string Encode() => "LK1|"+((int)Restored).ToString(CultureInfo.InvariantCulture)+"|"+Mirrors+"|"+Checkpoint+"|"+(HarborSlash?1:0);
        public static Journey Decode(string payload) {
            if(String.IsNullOrEmpty(payload))return new Journey();
            var fields=payload.Split('|');int flags,mirrors,checkpoint,harbor;
            if(fields.Length!=5 || fields[0]!="LK1" || !Int32.TryParse(fields[1],out flags) || !Int32.TryParse(fields[2],out mirrors) || !Int32.TryParse(fields[3],out checkpoint) || !Int32.TryParse(fields[4],out harbor) || flags<0 || flags>511 || mirrors<0 || mirrors>7 || checkpoint<0 || checkpoint>2 || harbor<0 || harbor>1)
                throw new InvalidOperationException("This journey is damaged or from a newer chapter. It has been kept.");
            var j=new Journey{Restored=(Restoration)flags,Mirrors=mirrors,Checkpoint=checkpoint,HarborSlash=harbor==1};
            var ordered=new[]{Restoration.Harbor,Restoration.Wrench,Restoration.Pump,Restoration.Lens,Restoration.Mounted,Restoration.Relay,Restoration.Beacon};
            for(int i=1;i<ordered.Length;i++)if(j.Has(ordered[i]) && !j.Has(ordered[i-1]))throw new InvalidOperationException("Journey restoration order is invalid. Original save kept.");
            if((checkpoint>0 && !j.Has(Restoration.Harbor)) || (checkpoint>1 && !j.Has(Restoration.Pump)))throw new InvalidOperationException("Journey checkpoint is invalid. Original save kept.");
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
        public bool Reached,Loop;
    }
    public static class Optics {
        public static Cell Reflect(Cell direction,bool slash)=>slash?new Cell(direction.Y,direction.X):new Cell(-direction.Y,-direction.X);
        // Discrete optics, with authored occluders. Bound and visited states stop loops.
        public static LightPath Trace(Cell emitter,Cell direction,Cell receiver,IDictionary<Cell,bool> mirrors,ISet<Cell> blocked,int minX,int maxX,int minY,int maxY) {
            var result=new LightPath();var seen=new HashSet<string>();var p=emitter;
            for(int i=0;i<256;i++) {
                p=new Cell(p.X+direction.X,p.Y+direction.Y);
                if(p.X<minX || p.X>maxX || p.Y<minY || p.Y>maxY || blocked.Contains(p))return result;
                var key=p.X+","+p.Y+","+direction.X+","+direction.Y;
                if(!seen.Add(key)){result.Loop=true;return result;}
                result.Steps.Add(new RayStep(p,direction));
                if(p.Equals(receiver)){result.Reached=true;return result;}
                bool slash;if(mirrors.TryGetValue(p,out slash))direction=Reflect(direction,slash);
            }
            result.Loop=true;return result;
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
