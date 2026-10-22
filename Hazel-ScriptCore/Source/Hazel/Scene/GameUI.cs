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
    public class UIButtonComponent : Component {
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
