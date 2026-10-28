using System;
using Hazel;

namespace Sandbox {
    // The scene owns geometry, values, focus, popups and input. This owner only
    // connects those values to a tiny preview and useful author-facing feedback.
    public class UITutorial : Entity {
        public Entity Feedback,Pages,Dialog,DialogButton,Progress,Toggle,Slider;
        public Entity Palette,Ocean,Amber,Cloud,Choice,NameField,NoteField,Journal,Corpus;
        private float amount=25;
        private bool enabled=true;
        private TextComponent feedback;
        void OnCreate() {
            feedback=Feedback.GetComponent<TextComponent>();
            Progress.GetComponent<UIProgressComponent>().Value=amount/100;
        }
        void Show(string message) {feedback.Text=message;}
        void Amount(float value) {
            amount=value;
            Progress.GetComponent<UIProgressComponent>().Value=amount/100;
        }
        void OnUpdate(float dt) {
            UIEvent e;
            while(GameUI.TryConsumeEvent(out e)) {
                if(e.Kind==UIEventKind.Activated) {
                    switch(e.Action) {
                        case "open-dialog": Dialog.GetComponent<UICanvasComponent>().Open();break;
                        case "close-dialog": Dialog.GetComponent<UICanvasComponent>().Close();break;
                        case "confirm": Amount(Math.Min(100,amount+10));Dialog.GetComponent<UICanvasComponent>().Close();break;
                        case "advance": Amount(amount>=100?0:Math.Min(100,amount+10));break;
                        case "silent":
                            Slider.GetComponent<UISliderComponent>().Value=50;Amount(50);
                            Show("Silent setter: 50. No user-change event was generated.");continue;
                        default:break;
                    }
                }
                else if(e.Kind==UIEventKind.ToggleChanged) enabled=e.Checked;
                else if(e.Kind==UIEventKind.SliderChanged || e.Kind==UIEventKind.SliderCancelled || (e.Kind==UIEventKind.SliderCommitted && e.Interrupted))
                    Amount(e.Interrupted?e.Source.GetComponent<UISliderComponent>().Value:e.Value);
                else if(e.Kind==UIEventKind.SelectionChanged && e.Source.ID==Palette.ID) {
                    var color=e.Selection!=null&&e.Selection.ID==Amber.ID?new Vector4(1,.75f,.3f,1):e.Selection!=null&&e.Selection.ID==Cloud.ID?new Vector4(.92f,.95f,1,1):new Vector4(.3f,.9f,.8f,1);
                    feedback.Color=color;
                }
                if(enabled) Show(e.Kind+" · "+e.Action+(e.Kind==UIEventKind.TextChanged?" · "+e.Text: e.Kind==UIEventKind.SliderChanged?" · "+e.Value.ToString("0"):""));
            }
        }
    }
}
