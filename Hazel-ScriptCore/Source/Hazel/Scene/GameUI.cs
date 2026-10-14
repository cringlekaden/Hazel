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
    public class UIRectComponent : Component {
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
