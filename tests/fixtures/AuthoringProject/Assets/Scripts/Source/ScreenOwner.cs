using Hazel;
namespace Migration {
    // Also used by the maintained native-authored screen fixture.
    public class ScreenOwnerProbe : Hazel.Entity {
        public Hazel.Entity Settings, Confirmation, Toggle, Choices, Progress, Volume, Level, Feedback, Scrolling, ScrollView, ScrollTarget, Dropdown, DropdownChoice, Tabbed, Tabs, JournalTab, SettingsTab, JournalList, JournalChoice, NameField, NoteField, NotesTab;
        public float LiveValue,CommittedValue;public int SliderChanges,SliderCommits,SliderCancels;
        public int ValueChanges;public bool LastPreviousChecked,LastChecked;public ulong LastPreviousSelection,LastSelection;
        public int TextChanges,TextCommits,TextCancels;public string LastText;
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
                    if(Volume==null||Level==null)throw new System.Exception("Package sliders missing");
                    var volume=Volume.GetComponent<UISliderComponent>();volume.Value=57;if(volume.Value!=55)throw new System.Exception("Package slider quantization");volume.Value=50;
                    var level=Level.GetComponent<UISliderComponent>();level.Value=.37f;if(level.Value!=.25f)throw new System.Exception("Package vertical slider quantization");level.Value=0;
                    System.Console.WriteLine("SLIDER-PACKAGE: canonical parts and managed silent quantization ready");
                    System.Console.WriteLine("VALUE-PACKAGE: typed controls and silent setters ready");settings.Open();
                }
                else if(packageStep==1){if(!settings.IsOpen)throw new System.Exception("Package settings did not open");confirmation.Open();}
                else if(packageStep==2){if(!confirmation.IsOpen)throw new System.Exception("Package confirmation did not open");confirmation.Close();}
                else if(packageStep==3){if(confirmation.IsOpen||!settings.IsOpen||Scrolling==null)throw new System.Exception("Package confirmation/scroll refs");Scrolling.GetComponent<UICanvasComponent>().Open();}
                else if(packageStep==4){var view=ScrollView.GetComponent<UIScrollViewComponent>();if(view.Range.Y<=0)throw new System.Exception("Package scroll extent");view.Normalized=new Vector2(0,.25f);if(!view.EnsureVisible(ScrollTarget))throw new System.Exception("Package nested reveal");System.Console.WriteLine("SCROLL-PACKAGE: static extent, offsets and nested reveal ready");if(Dropdown==null||!view.EnsureVisible(Dropdown))throw new System.Exception("Package dropdown trigger reveal");Dropdown.GetComponent<UIDropdownComponent>().Open();}
                else if(packageStep==5){var d=Dropdown.GetComponent<UIDropdownComponent>();if(!d.IsOpen)throw new System.Exception("Package dropdown popup");d.Selection=DropdownChoice;if(d.Selection.ID!=DropdownChoice.ID)throw new System.Exception("Package dropdown value");d.Close();}
                else if(packageStep==6){if(Dropdown.GetComponent<UIDropdownComponent>().IsOpen||!Scrolling.GetComponent<UICanvasComponent>().IsOpen)throw new System.Exception("Package owned popup return");System.Console.WriteLine("DROPDOWN-PACKAGE: owned popup and silent selection ready");Scrolling.GetComponent<UICanvasComponent>().Close();}
                else if(packageStep==7){if(Tabbed==null||Tabs==null)throw new System.Exception("Package tabs missing");Tabbed.GetComponent<UICanvasComponent>().Open();}
                else if(packageStep==8){Tabs.GetComponent<UITabsComponent>().Selection=JournalTab;}
                else if(packageStep==9){if(Tabs.GetComponent<UITabsComponent>().Selection.ID!=JournalTab.ID)throw new System.Exception("Package page switch");var list=JournalList.GetComponent<UIRadioGroupComponent>();list.Selection=JournalChoice;JournalChoice.GetComponent<UITooltipComponent>().Body.GetComponent<TextComponent>().Text="This entry can be inspected without losing your place.";Tabs.GetComponent<UITabsComponent>().Selection=NotesTab;}
                else if(packageStep==10){if(JournalList.GetComponent<UIRadioGroupComponent>().Selection.ID!=JournalChoice.ID||Tabs.GetComponent<UITabsComponent>().Selection.ID!=NotesTab.ID)throw new System.Exception("Package list/notes page");var appearance=NameField.GetComponent<UIAppearanceComponent>();var selectedStyle=appearance.Style;if(!selectedStyle.Enabled||selectedStyle.Asset!="Styles/Settings.hstyle"||selectedStyle.Variant!="Field")throw new System.Exception("Package style selection");appearance.Style=selectedStyle;NameField.GetComponent<UITextEntryComponent>().Text="Relocated name é";NoteField.GetComponent<UITextEntryComponent>().Text="Relocated note\nSecond line";}
                else if(packageStep==11){if(NameField.GetComponent<UITextEntryComponent>().Text!="Relocated name é"||NoteField.GetComponent<UITextEntryComponent>().Text!="Relocated note\nSecond line")throw new System.Exception("Package text-entry value");var emphasis=FindEntityByName("Emphasis journal").GetComponent<TextComponent>();
                    if(emphasis.Emphasis.Length!=2||emphasis.Emphasis[0].Face!=FontStyle.Bold||emphasis.Emphasis[1].Face!=FontStyle.Italic)throw new System.Exception("Package rich-text source/span face closure");
                    System.Console.WriteLine("EMPHASIS-PACKAGE: real bold/italic/color, source and typed spans ready");
                    System.Console.WriteLine("STYLE-PACKAGE: shared family/style references and independent runtime values ready");System.Console.WriteLine("TEXT-ENTRY-PACKAGE: canonical fields and silent text ready");Tabs.GetComponent<UITabsComponent>().Selection=SettingsTab;}
                else if(packageStep==12){if(Tabs.GetComponent<UITabsComponent>().Selection.ID!=SettingsTab.ID)throw new System.Exception("Package page return");System.Console.WriteLine("COMPOSITION-PACKAGE: tabs, list selection and tooltip content ready");Tabbed.GetComponent<UICanvasComponent>().Close();}
                else if(packageStep==13){if(Scrolling.GetComponent<UICanvasComponent>().IsOpen||!settings.IsOpen)throw new System.Exception("Package scroll scope return");settings.Close();}
                else {if(settings.IsOpen||!GetComponent<UICanvasComponent>().IsOpen)throw new System.Exception("Package menu return failed");System.Console.WriteLine("SCREEN-PACKAGE: nested open/close complete");packageProbe=false;}
                ++packageStep;
            }
            if(Progress!=null){var meter=Progress.GetComponent<UIProgressComponent>();meter.Value=meter.Value+dt*.05f;}
            Hazel.UIEvent value;
            while(Hazel.GameUI.TryConsumeEvent(out value)){
                if(value.Owner==null||value.Owner.ID!=ID)throw new System.Exception("Declared screen recipient mismatch");
                if(value.Kind==UIEventKind.TextChanged){++TextChanges;LastText=value.Text;continue;}
                if(value.Kind==UIEventKind.TextCommitted){++TextCommits;LastText=value.Text;continue;}
                if(value.Kind==UIEventKind.TextCancelled){++TextCancels;LastText=value.Text;continue;}
                ++Count;LastSource=value.Source.ID;LastScreen=value.Screen.ID;LastKind=(int)value.Kind;
                if(value.Kind==UIEventKind.SliderChanged){++SliderChanges;LiveValue=value.Value;if(Feedback!=null)Feedback.GetComponent<TextComponent>().Text="Live "+value.Action+": "+value.Value.ToString("0.##");continue;}
                if(value.Kind==UIEventKind.SliderCommitted){++SliderCommits;CommittedValue=value.Value;if(Feedback!=null)Feedback.GetComponent<TextComponent>().Text="Committed "+value.Action+": "+value.Value.ToString("0.##");continue;}
                if(value.Kind==UIEventKind.SliderCancelled){++SliderCancels;LiveValue=value.Interrupted?value.Source.GetComponent<UISliderComponent>().Value:value.Value;if(Feedback!=null)Feedback.GetComponent<TextComponent>().Text=(value.Interrupted?"Interrupted ":"Restored ")+value.Action+": "+LiveValue.ToString("0.##");continue;}
                if(value.Kind==Hazel.UIEventKind.ToggleChanged){++ValueChanges;LastPreviousChecked=value.PreviousChecked;LastChecked=value.Checked;continue;}
                if(value.Kind==Hazel.UIEventKind.SelectionChanged){++ValueChanges;LastPreviousSelection=value.PreviousSelection==null?0:value.PreviousSelection.ID;LastSelection=value.Selection==null?0:value.Selection.ID;continue;}
                if(value.Kind==Hazel.UIEventKind.Cancel){value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();continue;}
                if(value.Action=="tabs-settings"&&Tabbed!=null){Tabbed.GetComponent<UICanvasComponent>().Open();continue;}
                if(value.Action=="scroll-settings"&&Scrolling!=null){Scrolling.GetComponent<UICanvasComponent>().Open();continue;}
                if(value.Action=="scroll-reveal"&&ScrollView!=null){ScrollView.GetComponent<UIScrollViewComponent>().EnsureVisible(ScrollTarget,true);continue;}
                if(value.Action=="settings"&&Settings!=null)Settings.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="confirm"&&Confirmation!=null)Confirmation.GetComponent<Hazel.UICanvasComponent>().Open();
                else if(value.Action=="back")value.Screen.GetComponent<Hazel.UICanvasComponent>().Close();
            }
        }
    }
}
