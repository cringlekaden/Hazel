using System;
namespace Hazel {
    // UI actions never retain a delegate or managed object in the engine.
    public static class GameUI {
        public static bool IsPointerCaptured => InternalCalls.UI_IsPointerCaptured();
        public static bool IsKeyDown(KeyCode key) => InternalCalls.UI_IsKeyDown(key);
        public static bool TryConsumeAction(out Entity button,out string action) {
            ulong id;bool result=InternalCalls.UI_Consume(InternalCalls.Entity_GetSceneIdentity(),0,out id,out action);
            button=result?new Entity(id):null;return result;
        }
    }
    public class UICanvasComponent : Component {
        public bool Visible {get=>InternalCalls.UI_GetValue(Entity.CheckedID,0);set=>InternalCalls.UI_SetValue(Entity.CheckedID,0,value);}
        public bool Enabled {get=>InternalCalls.UI_GetValue(Entity.CheckedID,1);set=>InternalCalls.UI_SetValue(Entity.CheckedID,1,value);}
        public bool Modal {get=>InternalCalls.UI_GetValue(Entity.CheckedID,2);set=>InternalCalls.UI_SetValue(Entity.CheckedID,2,value);}
    }
    public enum UISizeMode { Fixed, Fill, Content }
    public struct UISizing {
        public UISizeMode Mode;
        public float FixedSize, Minimum;
        public float? Maximum;
        public UISizing(UISizeMode mode,float fixedSize=0,float minimum=0,float? maximum=null) {
            Mode=mode;FixedSize=fixedSize;Minimum=minimum;Maximum=maximum;
        }
    }
    public class UIRectComponent : Component {
        public uint LayoutVersion => InternalCalls.UI_LayoutVersion(Entity.CheckedID);
        public UISizing Width {get=>ReadSizing(0);set=>WriteSizing(0,value);}
        public UISizing Height {get=>ReadSizing(1);set=>WriteSizing(1,value);}
        UISizing ReadSizing(int axis) {
            int mode,has;float size,min,max;
            InternalCalls.UI_GetSizing(Entity.CheckedID,axis,out mode,out size,out min,out has,out max);
            return new UISizing((UISizeMode)mode,size,min,has!=0?(float?)max:null);
        }
        void WriteSizing(int axis,UISizing value) {
            InternalCalls.UI_SetSizing(Entity.CheckedID,axis,(int)value.Mode,value.FixedSize,value.Minimum,value.Maximum.HasValue?1:0,value.Maximum??0);
        }
        public bool Visible {get=>InternalCalls.UI_GetValue(Entity.CheckedID,4);set=>InternalCalls.UI_SetValue(Entity.CheckedID,4,value);}
        public bool Enabled {get=>InternalCalls.UI_GetValue(Entity.CheckedID,5);set=>InternalCalls.UI_SetValue(Entity.CheckedID,5,value);}
    }
    public class UIButtonComponent : Component {
        public bool Interactable {get=>InternalCalls.UI_GetValue(Entity.CheckedID,3);set=>InternalCalls.UI_SetValue(Entity.CheckedID,3,value);}
        public bool TryConsumeAction(out string action) {
            ulong id;var checkedID=Entity.CheckedID;
            return InternalCalls.UI_Consume(InternalCalls.Entity_GetSceneIdentity(),checkedID,out id,out action);
        }
    }
    public class UILayoutComponent : Component {}
    public class UITextStyleComponent : Component {}
    public class UINineSliceComponent : Component {}
}
