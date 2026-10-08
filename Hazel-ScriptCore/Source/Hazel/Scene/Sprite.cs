using System;
namespace Hazel {
    // Stable asset identity, separate from a region/clip's editable display name.
    public sealed class Sprite {
        public readonly string Sheet;
        public readonly ulong ID;
        public bool IsAssigned { get { return !String.IsNullOrEmpty(Sheet) && ID != 0; } }
        public Sprite(string sheet, ulong id) { Sheet = sheet ?? ""; ID = id; }
    }
    public sealed class SpriteAnimation {
        public readonly string Sheet;
        public readonly ulong ID;
        public bool IsAssigned { get { return !String.IsNullOrEmpty(Sheet) && ID != 0; } }
        public SpriteAnimation(string sheet, ulong id) { Sheet = sheet ?? ""; ID = id; }
    }
    public class SpriteRendererComponent : Component {
        // Selection is authored static content; an assigned animation supplies the rendered override.
        public void SetSprite(Sprite sprite) {
            if(sprite == null || !sprite.IsAssigned) throw new ArgumentException("Select an assigned sprite");
            InternalCalls.SpriteRendererComponent_SetSprite(Entity.CheckedID, sprite.Sheet, sprite.ID);
        }
    }
    public class SpriteAnimationComponent : Component {
        public void Play(SpriteAnimation clip) {
            if(clip == null || !clip.IsAssigned) throw new ArgumentException("Select an assigned clip");
            InternalCalls.SpriteAnimationComponent_Play(Entity.CheckedID, clip.Sheet, clip.ID);
        }
        public void Pause() { InternalCalls.SpriteAnimationComponent_Control(Entity.CheckedID, 0); }
        public void Resume() { InternalCalls.SpriteAnimationComponent_Control(Entity.CheckedID, 1); }
        public void Stop() { InternalCalls.SpriteAnimationComponent_Control(Entity.CheckedID, 2); }
        public bool IsPlaying { get { return InternalCalls.SpriteAnimationComponent_State(Entity.CheckedID, false); } }
        public bool IsFinished { get { return InternalCalls.SpriteAnimationComponent_State(Entity.CheckedID, true); } }
    }
}
