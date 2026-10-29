using System;
using System.Globalization;
using Hazel;
namespace LastLightkeeper {
    // Player presentation settings have their own slot. The journey format and
    // engine-ui-bindings remain owned by their existing services.
    public class Settings : Entity {
        public Entity Mute,Volume,Pace,Deliberate,Standard,Quick,Stick,LeftStick,NoStick,RightStick,SettingsStatus;
        private float volume=1;
        private bool muted;
        private TextComponent status;
        void OnCreate() {
            status=SettingsStatus.GetComponent<TextComponent>();
            try {
                string raw=SaveData.Read("presentation");
                if(!String.IsNullOrEmpty(raw)) {
                    string[] p=raw.Split('|');float v;
                    if(p.Length!=3||p[0]!="UI1"||!float.TryParse(p[1],NumberStyles.Float,CultureInfo.InvariantCulture,out v)||float.IsNaN(v)||float.IsInfinity(v)||v<0||v>1||(p[2]!="0"&&p[2]!="1"))throw new FormatException("Unsupported presentation settings; original file retained.");
                    volume=v;muted=p[2]=="1";
                }
            } catch(Exception e) {status.Text="Settings unreadable; using defaults. Original kept until your next edit.";Console.WriteLine("Lightkeeper settings: "+e.Message);}
            Sync();ApplySound();
        }
        void Sync() {
            Volume.GetComponent<UISliderComponent>().Value=volume;
            Mute.GetComponent<UIButtonComponent>().Checked=muted;
            var input=GameUI.InputSettings;
            Pace.GetComponent<UIRadioGroupComponent>().Selection=input.RepeatInterval>=.2f?Deliberate:input.RepeatInterval<.08f?Quick:Standard;
            Stick.GetComponent<UIDropdownComponent>().Selection=input.Stick<0?NoStick:input.Stick==1?RightStick:LeftStick;
        }
        void ApplySound(){Game.AudioVolume=muted?0:volume;}
        void Save() {
            try {SaveData.Write("presentation","UI1|"+volume.ToString("R",CultureInfo.InvariantCulture)+"|"+(muted?"1":"0"));status.Text=SaveData.IsPersistent?"Applied and saved for this project.":"Applied for this Play session; player files unchanged.";}
            catch(Exception e){status.Text="Applied for now; could not save. Check disk/write access, then retry.";Console.WriteLine("Lightkeeper settings: "+e.Message);}
        }
        void OnUpdate(float dt) {
            UIEvent e;
            while(GameUI.TryConsumeEvent(out e)) {
                if(e.Kind==UIEventKind.Activated) {
                    if(e.Action=="settings-back")GetComponent<UICanvasComponent>().Close();
                    else if(e.Action=="settings-reset") {
                        volume=1;muted=false;ApplySound();Save();
                        try {GameUI.ResetBindings();Sync();}
                        catch(Exception ex){status.Text="Sound reset; input reset failed: "+ex.Message;}
                    }
                } else if(e.Kind==UIEventKind.ToggleChanged && e.Source.ID==Mute.ID) {muted=e.Checked;ApplySound();Save();}
                else if(e.Source.ID==Volume.ID) {
                    if(e.Kind==UIEventKind.SliderChanged||e.Kind==UIEventKind.SliderCommitted||e.Kind==UIEventKind.SliderCancelled){
                        // A replacement can retire an older edit. Its historical
                        // terminal payload must not overwrite the newer value.
                        volume=e.Interrupted?Volume.GetComponent<UISliderComponent>().Value:e.Value;ApplySound();
                        if(e.Kind==UIEventKind.SliderChanged)status.Text="Volume preview; release/confirm saves. Cancel restores.";
                        else if(e.Interrupted)status.Text="Preview interrupted; start and confirm a volume edit to save.";
                        else Save();
                    }
                } else if(e.Kind==UIEventKind.SelectionChanged) {
                    try {
                        var input=GameUI.InputSettings;
                        if(e.Source.ID==Pace.ID){input.RepeatDelay=e.Selection!=null&&e.Selection.ID==Deliberate.ID ? .6f : e.Selection!=null&&e.Selection.ID==Quick.ID ? .25f : .4f;input.RepeatInterval=e.Selection!=null&&e.Selection.ID==Deliberate.ID ? .25f : e.Selection!=null&&e.Selection.ID==Quick.ID ? .06f : .12f;}
                        else if(e.Source.ID==Stick.ID)input.Stick=e.Selection!=null&&e.Selection.ID==NoStick.ID?-1:e.Selection!=null&&e.Selection.ID==RightStick.ID?1:0;
                        else continue;
                        GameUI.InputSettings=input;status.Text=SaveData.IsPersistent?"Navigation override saved.":"Navigation override applied to this Play session.";
                    }catch(Exception ex){status.Text="Navigation change rejected: "+ex.Message;Sync();}
                }
            }
        }
    }
}
