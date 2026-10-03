using System;
namespace Skybound {
    // Gameplay model: a fixed pool and fixed timestep, independent of rendering.
    public sealed class Flight {
        public enum State { Ready, Flying, Dead }
        public sealed class Gate {
            public float X { get; internal set; }
            public float Gap { get; internal set; }
            internal bool Scored;
        }
        public const float BirdX=-3, BirdRadius=0.24f, GateHalfWidth=0.55f, Spacing=5.5f;
        public const double Step=1.0/120;
        public readonly Gate[] Gates=new Gate[4];
        public State Phase { get; private set; }
        public float Y { get; private set; }
        public float Velocity { get; private set; }
        public int Score { get; private set; }
        public float GapHalf { get; private set; }
        private readonly float speed, gravity, flap;
        private readonly Random random=new Random(1337);
        private double accumulated;
        private float lastGap=.25f;
        public Flight(float speed=2.8f,float gravity=9.6f,float flap=5,float gapHalf=1.7f) {
            this.speed=Clamp(speed,1,5);this.gravity=Clamp(gravity,4,20);this.flap=Clamp(flap,2,9);GapHalf=Clamp(gapHalf,1.4f,2.4f);
            for(int i=0;i<Gates.Length;i++) Gates[i]=new Gate{X=4+i*Spacing,Gap=i==0?0:.25f};
        }
        static float Clamp(float x,float low,float high) { return float.IsNaN(x)||float.IsInfinity(x)?low:Math.Max(low,Math.Min(high,x)); }
        public void Advance(float seconds,bool flapPressed) {
            if(Phase==State.Dead)return;
            if(flapPressed) { Phase=State.Flying;Velocity=flap; }
            if(Phase==State.Ready)return;
            // Treat suspended/dragged windows as a pause, not an unfair giant step.
            accumulated+=Clamp(seconds,0,.25f);
            while(accumulated+1e-9>=Step&&Phase==State.Flying) {
                accumulated-=Step;
                float dt=(float)Step;
                Velocity=Math.Max(-8,Velocity-gravity*dt);Y+=Velocity*dt;
                if(Y-BirdRadius<-4||Y+BirdRadius>4.2f) { Phase=State.Dead;break; }
                foreach(var gate in Gates) {
                    gate.X-=speed*dt;
                    if(Math.Abs(gate.X-BirdX)<GateHalfWidth+BirdRadius &&
                       (Y+BirdRadius>gate.Gap+GapHalf||Y-BirdRadius<gate.Gap-GapHalf)) { Phase=State.Dead;break; }
                    if(!gate.Scored&&gate.X+GateHalfWidth<BirdX-BirdRadius) { gate.Scored=true;Score++; }
                }
                foreach(var gate in Gates) if(gate.X<-10) {
                    float furthest=-10;
                    foreach(var other in Gates)furthest=Math.Max(furthest,other.X);
                    gate.X=furthest+Spacing;
                    float next=(float)(random.NextDouble()*2-1);
                    gate.Gap=Clamp(next,lastGap-.8f,lastGap+.8f);lastGap=gate.Gap;gate.Scored=false;
                }
            }
        }
    }
}
