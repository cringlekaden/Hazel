using Hazel;
namespace Migration {
    // Also used by the maintained native-authored screen fixture.
    public class ScreenOwnerProbe : Hazel.Entity {
        public Hazel.Entity Settings, Confirmation, Toggle, Choices, Progress;
        public int ValueChanges;public bool LastPreviousChecked,LastChecked;public ulong LastPreviousSelection,LastSelection;
        public int Count;public ulong LastSource,LastScreen;public int LastKind;
        bool packageProbe;int packageStep;
        void OnCreate(){packageProbe=System.Environment.GetEnvironmentVariable("HAZEL_UI_SCREEN_PACKAGE_PROBE")=="1";}
        void OnUpdate(float dt){
            // Bounded relocated-package lifecycle check, independent of physical input.
            if(packageProbe){
                if(Settings==null||Confirmation==null)throw new System.Exception("Package screen fixture references missing");
                var settings=Settings.GetComponent<UICanvasComponent>();var confirmation=Confirmation.GetComponent<UICanvasComponent>();
                if(packageStep==0){
                    if(Toggle==null||Choices==null||Progress==null)throw new System.Exception("Package value controls missing");
                    var toggle=Toggle.GetComponent<UIButtonComponent>();bool initial=toggle.Checked;toggle.Checked=!initial;toggle.Checked=initial;
                    var group=Choices.GetComponent<UIRadioGroupComponent>();var selected=group.Selection;
                    if(selected==null||Choices.Children.Length!=3)throw new System.Exception("Package radio initial/membership");
                    group.Selection=Choices.Children[1];group.Selection=selected;
                    var meter=Progress.GetComponent<UIProgressComponent>();meter.Value=2;if(meter.Value!=1)throw new System.Exception("Package progress clamp");meter.Value=.25f;
                    System.Console.WriteLine("VALUE-PACKAGE: typed controls and silent setters ready");settings.Open();
                }
                else if(packageStep==1){if(!settings.IsOpen)throw new System.Exception("Package settings did not open");confirmation.Open();}
                else if(packageStep==2){if(!confirmation.IsOpen)throw new System.Exception("Package confirmation did not open");confirmation.Close();}
                else if(packageStep==3){if(confirmation.IsOpen||!settings.IsOpen)throw new System.Exception("Package confirmation did not close independently");settings.Close();}
                else {if(settings.IsOpen||!GetComponent<UICanvasComponent>().IsOpen)throw new System.Exception("Package menu return failed");System.Console.WriteLine("SCREEN-PACKAGE: nested open/close complete");packageProbe=false;}
                ++packageStep;
            }
            if(Progress!=null){var meter=Progress.GetComponent<UIProgressComponent>();meter.Value=meter.Value+dt*.05f;}
            Hazel.UIEvent value;
            while(Hazel.GameUI.TryConsumeEvent(out value)){
                if(value.Owner==null||value.Owner.ID!=ID)throw new System.Exception("Declared screen recipient mismatch");
                ++Count;LastSource=value.Source.ID;LastScreen=value.Screen.ID;LastKind=(int)value.Kind;
                if(value.Kind==Hazel.UIEventKind.ToggleChanged){++ValueChanges;LastPreviousChecked=value.PreviousChecked;LastChecked=value.Checked;continue;}
                if(value.Kind==Hazel.UIEventKind.SelectionChanged){++ValueChanges;LastPreviousSelection=value.PreviousSelection==null?0:value.PreviousSelection.ID;LastSelection=value.Selection==null?0:value.Selection.ID;continue;}
                if(value.Kind==Hazel.UIEventKind.Cancel){value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();continue;}
                if(value.Action=="settings"&&Settings!=null)Settings.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="confirm"&&Confirmation!=null)Confirmation.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="back")value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();
            }
        }
    }
}
