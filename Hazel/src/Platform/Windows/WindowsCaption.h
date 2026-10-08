#pragma once
#include "Hazel/Core/Base.h"
#include <Windows.h>
#include <string>
namespace Hazel {
class Window;
// Per-HWND bridge: never changes global GLFW titlebar hints or detached
// windows.
class WindowsCaption {
  public:
    static Scope<WindowsCaption> Create(Window &owner, std::string &reason);
    ~WindowsCaption();
    bool NeedsNativeFallback() const { return m_Fallback; }

  private:
    explicit WindowsCaption(Window &owner) : m_Owner(owner) {}
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);
    Window &m_Owner;
    HWND m_Handle = nullptr;
    WNDPROC m_Previous = nullptr;
    bool m_MaxPressed = false, m_Fallback = false;
};
} // namespace Hazel
