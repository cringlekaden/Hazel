using Hazel;
namespace Migration {
    // Also used by the maintained native-authored screen fixture.
    public class ScreenOwnerProbe : Hazel.Entity {
        public Hazel.Entity Settings, Confirmation;
        public int Count;public ulong LastSource,LastScreen;public int LastKind;
        bool packageProbe;int packageStep;
        void OnCreate(){packageProbe=System.Environment.GetEnvironmentVariable("HAZEL_UI_SCREEN_PACKAGE_PROBE")=="1";}
        void OnUpdate(float dt){
            // Bounded relocated-package lifecycle check, independent of physical input.
            if(packageProbe){
                if(Settings==null||Confirmation==null)throw new System.Exception("Package screen fixture references missing");
                var settings=Settings.GetComponent<UICanvasComponent>();var confirmation=Confirmation.GetComponent<UICanvasComponent>();
                if(packageStep==0)settings.Open();
                else if(packageStep==1){if(!settings.IsOpen)throw new System.Exception("Package settings did not open");confirmation.Open();}
                else if(packageStep==2){if(!confirmation.IsOpen)throw new System.Exception("Package confirmation did not open");confirmation.Close();}
                else if(packageStep==3){if(confirmation.IsOpen||!settings.IsOpen)throw new System.Exception("Package confirmation did not close independently");settings.Close();}
                else {if(settings.IsOpen||!GetComponent<UICanvasComponent>().IsOpen)throw new System.Exception("Package menu return failed");System.Console.WriteLine("SCREEN-PACKAGE: nested open/close complete");packageProbe=false;}
                ++packageStep;
            }
            Hazel.UIEvent value;
            while(Hazel.GameUI.TryConsumeEvent(out value)){
                ++Count;LastSource=value.Source.ID;LastScreen=value.Screen.ID;LastKind=(int)value.Kind;
                if(value.Kind==Hazel.UIEventKind.Cancel){value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();continue;}
                if(value.Action=="settings"&&Settings!=null)Settings.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="confirm"&&Confirmation!=null)Confirmation.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="back")value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();
            }
        }
    }
}
