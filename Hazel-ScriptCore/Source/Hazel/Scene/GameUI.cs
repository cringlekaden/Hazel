using System;
namespace Hazel {
    // UI actions never retain a delegate or managed object in the engine.
    public static class GameUI {
        public static bool Focus(Entity target)=>InternalCalls.UI_Focus(InternalCalls.Entity_GetSceneIdentity(),target==null?0:target.CheckedID);
        public static bool PopScreen()=>InternalCalls.UI_Screen(InternalCalls.Entity_GetSceneIdentity(),0,2);
        // The engine derives the consumer from the executing script; callers cannot steal another owner's events.
        public static bool TryConsumeEvent(out UIEvent value){
            ulong screen,source,sequence,previousSelection,selection,owner;int kind;string action;bool previousChecked,checkedValue,interrupted;float previousValue,currentValue,startValue;ulong gesture;
            bool result=InternalCalls.UI_ConsumeEvent(InternalCalls.Entity_GetSceneIdentity(),out screen,out source,out kind,out sequence,out action,out previousChecked,out checkedValue,out previousSelection,out selection,out owner,out previousValue,out currentValue,out startValue,out gesture,out interrupted);
            value=result?new UIEvent{Screen=screen==0?null:new Entity(screen),Source=new Entity(source),Owner=new Entity(owner),PreviousValue=previousValue,Value=currentValue,StartValue=startValue,Gesture=gesture,Interrupted=interrupted,Kind=(UIEventKind)kind,Sequence=sequence,Action=action,PreviousChecked=previousChecked,Checked=checkedValue,PreviousSelection=previousSelection==0?null:new Entity(previousSelection),Selection=selection==0?null:new Entity(selection)}:default(UIEvent);return result;
        }
        public static ulong FocusScope => InternalCalls.UI_FocusScope();
        public static bool TryConsumeCancel() => InternalCalls.UI_Cancel(InternalCalls.Entity_GetSceneIdentity(),0);
        public static UIBinding GetBinding(UIAction action) {int k=0,a=0,p=0,s=0;InternalCalls.UI_Binding((int)action,ref k,ref a,ref p,ref s,false);return new UIBinding(k,a,p,(UIShiftMatch)s);}
        public static void SetBinding(UIAction action,UIBinding binding) {int k=binding.Key,a=binding.Alternate,p=binding.GamepadButton,s=(int)binding.Shift;InternalCalls.UI_Binding((int)action,ref k,ref a,ref p,ref s,true);}
        public static UIInputSettings InputSettings {get{var v=new UIInputSettings();InternalCalls.UI_InputSettings(ref v,false);return v;}set{InternalCalls.UI_InputSettings(ref value,true);}}
        public static void ResetBindings()=>InternalCalls.UI_ResetBindings();
        public static bool IsPointerCaptured => InternalCalls.UI_IsPointerCaptured();
        public static bool IsKeyDown(KeyCode key) => InternalCalls.UI_IsKeyDown(key);
        public static bool TryConsumeAction(out Entity button,out string action) {
            ulong id;bool result=InternalCalls.UI_Consume(InternalCalls.Entity_GetSceneIdentity(),0,out id,out action);
            button=result?new Entity(id):null;return result;
        }
    }
    public enum UIEventKind { Activated, Cancel, ToggleChanged, SelectionChanged, SliderChanged, SliderCommitted, SliderCancelled }
    public struct UIEvent {public Entity Screen,Source,Owner;public UIEventKind Kind;public ulong Sequence;public string Action;public bool PreviousChecked,Checked;public Entity PreviousSelection,Selection;public float PreviousValue,Value,StartValue;public ulong Gesture;public bool Interrupted;}
    public enum UIAction { Press, Up, Down, Left, Right, Next, Previous, Accept, Cancel, AdjustDecrease, AdjustIncrease, Scroll, ScrollUp, ScrollDown, ScrollLeft, ScrollRight }
    public enum UIShiftMatch { Any, Without, With }
    public enum UIGamepadButton { None=-1,A,B,X,Y,LeftBumper,RightBumper,Back,Start,Guide,LeftThumb,RightThumb,Up,Right,Down,Left }
    public struct UIBinding {
        public int Key,Alternate,GamepadButton;public UIShiftMatch Shift;
        public UIBinding(int key,int alternate=-1,int gamepadButton=-1,UIShiftMatch shift=UIShiftMatch.Any){Key=key;Alternate=alternate;GamepadButton=gamepadButton;Shift=shift;}
    }
    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    public struct UIInputSettings {public int PointerButton,Stick;public float DeadZone,NeutralZone,RepeatDelay,RepeatInterval;}
    public class UICanvasComponent : Component {
        public bool IsOpen=>InternalCalls.UI_Screen(InternalCalls.Entity_GetSceneIdentity(),Entity.CheckedID,-1);
        public bool Open()=>InternalCalls.UI_Screen(InternalCalls.Entity_GetSceneIdentity(),Entity.CheckedID,0);
        public bool Close()=>InternalCalls.UI_Screen(InternalCalls.Entity_GetSceneIdentity(),Entity.CheckedID,1);
        public bool ReplaceTop()=>InternalCalls.UI_Screen(InternalCalls.Entity_GetSceneIdentity(),Entity.CheckedID,3);
        public bool TryConsumeCancel()=>InternalCalls.UI_Cancel(InternalCalls.Entity_GetSceneIdentity(),Entity.CheckedID);
        public bool Visible {get=>InternalCalls.UI_GetValue(Entity.CheckedID,0);set=>InternalCalls.UI_SetValue(Entity.CheckedID,0,value);}
        public bool Enabled {get=>InternalCalls.UI_GetValue(Entity.CheckedID,1);set=>InternalCalls.UI_SetValue(Entity.CheckedID,1,value);}
        public bool Modal {get=>InternalCalls.UI_GetValue(Entity.CheckedID,2);set=>InternalCalls.UI_SetValue(Entity.CheckedID,2,value);}
    }
    public enum UIImageFit { Stretch, Contain, Cover, Native }
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
        public UIImageFit ImageFit {get{int fit;Vector2 alignment;InternalCalls.UI_GetImageFit(Entity.CheckedID,out fit,out alignment);return (UIImageFit)fit;}set=>SetImageFit(value,ImageAlignment);}
        public Vector2 ImageAlignment {get{int fit;Vector2 alignment;InternalCalls.UI_GetImageFit(Entity.CheckedID,out fit,out alignment);return alignment;}set=>SetImageFit(ImageFit,value);}
        public void SetImageFit(UIImageFit fit,Vector2 alignment)=>InternalCalls.UI_SetImageFit(Entity.CheckedID,(int)fit,ref alignment);
        public UIGridCell Cell {get{int row,column,rs,cs;InternalCalls.UI_GetCell(Entity.CheckedID,out row,out column,out rs,out cs);return new UIGridCell(row,column,rs,cs);}set=>InternalCalls.UI_SetCell(Entity.CheckedID,value.Row,value.Column,value.RowSpan,value.ColumnSpan);}
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
    public class UIRadioGroupComponent : Component {
        public Entity Selection {get{ulong id=InternalCalls.UI_Selection(Entity.CheckedID,false,0);return id==0?null:new Entity(id);}set=>InternalCalls.UI_Selection(Entity.CheckedID,true,value==null?0:value.CheckedID);}
    }
    public class UIScrollViewComponent : Component {
        public Vector2 Offset {get{Vector2 offset,range,viewport,extent;InternalCalls.UI_ScrollInfo(Entity.CheckedID,out offset,out range,out viewport,out extent);return offset;}set{InternalCalls.UI_ScrollTo(Entity.CheckedID,ref value,false);}}
        public Vector2 Range {get{Vector2 offset,range,viewport,extent;InternalCalls.UI_ScrollInfo(Entity.CheckedID,out offset,out range,out viewport,out extent);return range;}}
        public Vector2 Normalized {get{var o=Offset;var r=Range;return new Vector2(r.X>0?o.X/r.X:0,r.Y>0?o.Y/r.Y:0);}set{InternalCalls.UI_ScrollTo(Entity.CheckedID,ref value,true);}}
        public bool EnsureVisible(Entity target,bool focus=false)=>InternalCalls.UI_EnsureVisible(Entity.CheckedID,target==null?0:target.CheckedID,focus);
    }
    public class UISliderComponent : Component {
        // Silent setter interrupts an edit before applying the quantized value.
        public float Value {get=>InternalCalls.UI_Slider(Entity.CheckedID,false,0);set=>InternalCalls.UI_Slider(Entity.CheckedID,true,value);}
    }
    public class UIProgressComponent : Component {
        public float Value {get=>InternalCalls.UI_Progress(Entity.CheckedID,false,0);set=>InternalCalls.UI_Progress(Entity.CheckedID,true,value);}
    }
    public class UIButtonComponent : Component {
        // Radio options are readable here; set selection through their group.
        public bool Checked {get=>InternalCalls.UI_Checked(Entity.CheckedID,false,false);set=>InternalCalls.UI_Checked(Entity.CheckedID,true,value);}
        public bool Interactable {get=>InternalCalls.UI_GetValue(Entity.CheckedID,3);set=>InternalCalls.UI_SetValue(Entity.CheckedID,3,value);}
        public bool TryConsumeAction(out string action) {
            ulong id;var checkedID=Entity.CheckedID;
            return InternalCalls.UI_Consume(InternalCalls.Entity_GetSceneIdentity(),checkedID,out id,out action);
        }
    }
    public enum UIContainerKind { Stack, Grid, Wrap }
    public enum UIAxis { Horizontal, Vertical }
    public enum UIAlign { Start, Center, End }
    public enum UITrackMode { Fixed, Fill }
    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    public struct UITrack {
        public UITrackMode Mode;public float Value,Minimum,Maximum;int hasMaximum;
        public UITrack(UITrackMode mode,float value,float minimum=0,float? maximum=null){Mode=mode;Value=value;Minimum=minimum;Maximum=maximum??0;hasMaximum=maximum.HasValue?1:0;}
        public float? Limit {get=>hasMaximum!=0?(float?)Maximum:null;set{hasMaximum=value.HasValue?1:0;Maximum=value??0;}}
    }
    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    public struct UILayoutSettings {
        public UIContainerKind Kind;public UIAxis Direction;public UIAlign Main,Cross,LineAlign;public float Gap,LineGap;public Vector2 GridGap;
        public static UILayoutSettings Stack(UIAxis direction,float gap=12)=>new UILayoutSettings{Kind=UIContainerKind.Stack,Direction=direction,Gap=gap,LineGap=12,GridGap=new Vector2(12,12)};
        public static UILayoutSettings Wrap(UIAxis direction,float gap=12,float lineGap=12){var settings=Stack(direction,gap);settings.Kind=UIContainerKind.Wrap;settings.LineGap=lineGap;return settings;}
        public static UILayoutSettings Grid(Vector2 gap)=>new UILayoutSettings{Kind=UIContainerKind.Grid,GridGap=gap,Gap=12,LineGap=12};
    }
    public struct UIGridCell {
        public int Row,Column,RowSpan,ColumnSpan;
        public UIGridCell(int row,int column,int rowSpan=1,int columnSpan=1){Row=row;Column=column;RowSpan=rowSpan;ColumnSpan=columnSpan;}
        public static UIGridCell Automatic=>new UIGridCell(-1,-1);
    }
    public class UILayoutComponent : Component {
        public UILayoutSettings Settings {get{UILayoutSettings value;InternalCalls.UI_GetLayout(Entity.CheckedID,out value);return value;}set=>Apply(value);}
        public UITrack[] Columns=>ReadTracks(false);
        public UITrack[] Rows=>ReadTracks(true);
        UITrack[] ReadTracks(bool rows){var id=Entity.CheckedID;var values=new UITrack[InternalCalls.UI_TrackCount(id,rows)];for(int i=0;i<values.Length;++i)InternalCalls.UI_GetTrack(id,rows,i,out values[i]);return values;}
        // Apply settings and both track lists atomically; null retains the existing list.
        public void Apply(UILayoutSettings settings,UITrack[] columns=null,UITrack[] rows=null)=>InternalCalls.UI_SetLayout(Entity.CheckedID,ref settings,columns,rows);
        public void SetGrid(UITrack[] columns,UITrack[] rows,Vector2 gap)=>Apply(UILayoutSettings.Grid(gap),columns,rows);
    }
    public enum UITextOverflow { Clip, Ellipsis }
    public class UITextStyleComponent : Component {
        public UITextOverflow Overflow {get{int mode=0;uint lines=0;InternalCalls.UI_TextOverflow(Entity.CheckedID,ref mode,ref lines,false);return (UITextOverflow)mode;}set=>SetOverflow(value,MaxLines);}
        // Zero is unlimited within the 4096-line engine limit.
        public uint MaxLines {get{int mode=0;uint lines=0;InternalCalls.UI_TextOverflow(Entity.CheckedID,ref mode,ref lines,false);return lines;}set=>SetOverflow(Overflow,value);}
        public void SetOverflow(UITextOverflow overflow,uint maxLines=0){int mode=(int)overflow;InternalCalls.UI_TextOverflow(Entity.CheckedID,ref mode,ref maxLines,true);}
    }
    public class UINineSliceComponent : Component {}
}
