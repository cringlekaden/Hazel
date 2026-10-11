using System;
using Hazel;
namespace LastLightkeeper {
    // Small game-owned pool of native authored prefabs. Terrain stays in scenes.
    internal sealed class LightRig {
        readonly Entity[] runs=new Entity[12],ends=new Entity[3];int run,end;
        public LightRig(Prefab beam,Prefab cap){
            for(int i=0;i<runs.Length;i++){runs[i]=Entity.Instantiate(beam,new Vector3(0,0,-9));Hide(runs[i]);}
            for(int i=0;i<ends.Length;i++){ends[i]=Entity.Instantiate(cap,new Vector3(0,0,-9));Hide(ends[i]);}
        }
        static void Hide(Entity e){e.Scale=new Vector3(.001f,.001f,1);}
        public void Begin(){run=end=0;}
        public void Draw(LightPath path,Cell emitter){
            if(path.Steps.Count==0 && !path.Obstructed)return;
            for(int begin=0;begin<path.Steps.Count && run<runs.Length;){
                int last=begin;var d=path.Steps[begin].Direction;
                while(last+1<path.Steps.Count && path.Steps[last+1].Direction.Equals(d))last++;
                var a=begin==0?emitter:path.Steps[begin-1].Position;var b=path.Steps[last].Position;
                var e=runs[run++];e.Translation=new Vector3((a.X+b.X)*.5f,(a.Y+b.Y)*.5f,.27f);
                e.Scale=new Vector3(Math.Abs(b.X-a.X)+Math.Abs(b.Y-a.Y)+.1f,.5f,1);
                e.LocalRotation=new Vector3(0,0,d.X>0?0:d.X<0?(float)Math.PI:d.Y>0?(float)Math.PI/2:-(float)Math.PI/2);
                begin=last+1;
            }
            if(end<ends.Length){
                var last=path.Steps.Count>0?path.Steps[path.Steps.Count-1]:new RayStep(path.End,path.EndDirection);var cap=ends[end++];
                if(path.Obstructed)last=new RayStep(path.End,path.EndDirection);
                cap.Translation=new Vector3(last.Position.X,last.Position.Y,.55f);cap.Scale=new Vector3(.65f,.65f,1);
                cap.GetComponent<SpriteRendererComponent>().SetSprite(path.Reached?Art.Region("tiny-dungeon",56):Art.Region("tiny-battle",path.Obstructed?192:43));
                cap.LocalRotation=new Vector3(0,0,path.Reached || path.Obstructed?0:last.Direction.X>0?0:last.Direction.X<0?(float)Math.PI:last.Direction.Y>0?(float)Math.PI/2:-(float)Math.PI/2);
            }
        }
        public void Finish(){for(int i=run;i<runs.Length;i++)Hide(runs[i]);for(int i=end;i<ends.Length;i++)Hide(ends[i]);}
        public void Destroy(){foreach(var e in runs)if(e!=null && e.IsValid)e.Destroy();foreach(var e in ends)if(e!=null && e.IsValid)e.Destroy();}
    }
}
