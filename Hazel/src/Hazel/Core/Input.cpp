#include "hzpch.h"
#include "Input.h"
#include "Application.h"
#include "Hazel/ImGui/ImGuiLayer.h"
namespace Hazel {
void* Input::NativeWindow() {
    auto& application = Application::Get();
    if (auto* ui = application.GetImGuiLayer()) return ui->InputWindow();
    return application.GetWindow().GetNativeWindow();
}
bool Input::IsFocused() {
    auto& application = Application::Get();
    if (auto* ui = application.GetImGuiLayer()) return ui->InputFocused();
    return application.GetWindow().IsFocused();
}
bool Input::IsPointerCaptured() {
    auto& application = Application::Get();
    if (auto* ui = application.GetImGuiLayer()) return ui->PointerCaptured();
    return application.GetWindow().IsPointerCaptured();
}
void Input::SetPointerCaptured(bool captured) {
    auto& application = Application::Get();
    if (auto* ui = application.GetImGuiLayer()) ui->CapturePointer(captured);
    else application.GetWindow().SetPointerCaptured(captured);
}
}
