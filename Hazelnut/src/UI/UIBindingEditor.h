#pragma once
#include "Hazel/UI/UIActions.h"
namespace Hazel {
// Settings form state, never a second input router or scene history.
struct UIBindingEditor {
    GameUI::UIBindings Values=GameUI::UIBindings::Defaults();
    int CaptureAction=-1,CaptureSlot=0;
    bool Neutral=false;
    std::string Error;
    void CancelCapture(){CaptureAction=-1;Neutral=false;}
    bool Capturing() const {return CaptureAction>=0;}
    void Draw(bool editable=true);
    bool Valid();
};
}
