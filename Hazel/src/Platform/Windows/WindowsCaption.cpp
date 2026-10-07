#include "hzpch.h"

#include "WindowsCaption.h"
#include "Hazel/Core/Window.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windowsx.h>
namespace Hazel {
namespace {
constexpr const wchar_t *Property = L"Hazel.MainCaption";
using DpiFunction = UINT(WINAPI *)(HWND);
using MetricFunction = int(WINAPI *)(int, UINT);
DpiFunction Dpi() {
    return reinterpret_cast<DpiFunction>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
}
MetricFunction Metrics() {
    return reinterpret_cast<MetricFunction>(GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
}
int Border(HWND hwnd, int metric) {
    return Metrics()(metric, Dpi()(hwnd)) +
           Metrics()(SM_CXPADDEDBORDER, Dpi()(hwnd));
}
using CompositionFunction = HRESULT(WINAPI *)(BOOL *);
using FrameFunction = HRESULT(WINAPI *)(HWND, const MARGINS *);
struct DesktopFrame {
    HMODULE Library = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    CompositionFunction Composition =
        Library ? reinterpret_cast<CompositionFunction>(
                      GetProcAddress(Library, "DwmIsCompositionEnabled"))
                : nullptr;
    FrameFunction Extend =
        Library ? reinterpret_cast<FrameFunction>(
                      GetProcAddress(Library, "DwmExtendFrameIntoClientArea"))
                : nullptr;
    ~DesktopFrame() {
        if (Library)
            FreeLibrary(Library);
    }
};
DesktopFrame &Frame() {
    static DesktopFrame value;
    return value;
}
void Refresh(HWND hwnd) {
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
}
} // namespace
Scope<WindowsCaption> WindowsCaption::Create(Window &owner,
                                             std::string &reason) {
    auto result = Scope<WindowsCaption>(new WindowsCaption(owner));
    result->m_Handle =
        glfwGetWin32Window(static_cast<GLFWwindow *>(owner.GetNativeWindow()));
    BOOL composed = FALSE;
    if (glfwGetWindowMonitor(
            static_cast<GLFWwindow *>(owner.GetNativeWindow())) ||
        !Dpi() || !Metrics() || !Frame().Composition || !Frame().Extend ||
        FAILED(Frame().Composition(&composed)) || !composed) {
        reason =
            "Native fallback: a windowed Windows 10+ desktop with DPI APIs "
            "and composition is required";
        return {};
    }
    if (GetPropW(result->m_Handle, Property) ||
        !SetPropW(result->m_Handle, Property, result.get())) {
        reason = "Native fallback: caption ownership unavailable";
        return {};
    }
    SetLastError(0);
    auto previous = SetWindowLongPtrW(result->m_Handle, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(&Procedure));
    if (!previous) {
        RemovePropW(result->m_Handle, Property);
        reason = "Native fallback: window procedure installation failed";
        return {};
    }
    result->m_Previous = reinterpret_cast<WNDPROC>(previous);
    MARGINS margins{1, 1, 1, 1};
    if (FAILED(Frame().Extend(result->m_Handle, &margins))) {
        reason = "Native fallback: DWM frame extension failed";
        return {};
    }
    Refresh(result->m_Handle);
    reason = "Custom Windows caption; OS move/resize/system menu and maximize "
             "hit region retained";
    return result;
}
WindowsCaption::~WindowsCaption() {
    if (m_Previous && IsWindow(m_Handle)) {
        SetWindowLongPtrW(m_Handle, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(m_Previous));
        RemovePropW(m_Handle, Property);
        MARGINS margins{};
        Frame().Extend(m_Handle, &margins);
        Refresh(m_Handle);
    }
}
LRESULT CALLBACK WindowsCaption::Procedure(HWND hwnd, UINT message, WPARAM w,
                                           LPARAM l) {
    auto *caption = static_cast<WindowsCaption *>(GetPropW(hwnd, Property));
    return caption ? caption->Handle(message, w, l)
                   : DefWindowProcW(hwnd, message, w, l);
}
LRESULT WindowsCaption::Handle(UINT message, WPARAM w, LPARAM l) {
    if (message == WM_DWMCOMPOSITIONCHANGED) {
        BOOL composed = FALSE;
        if (FAILED(Frame().Composition(&composed)) || !composed)
            m_Fallback = true;
    }
    if (message == WM_NCCALCSIZE && w) {
        auto &rect = reinterpret_cast<NCCALCSIZE_PARAMS *>(l)->rgrc[0];
        if (IsZoomed(m_Handle)) {
            MONITORINFO info{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(
                    MonitorFromWindow(m_Handle, MONITOR_DEFAULTTONEAREST),
                    &info))
                rect = info.rcWork;
        } else {
            const int border = Border(m_Handle, SM_CXFRAME);
            rect.left += border;
            rect.right -= border;
            rect.bottom -= Border(m_Handle, SM_CYFRAME);
            rect.top += 1;
        }
        return 0;
    }
    if (message == WM_NCHITTEST) {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(m_Handle, &p);
        RECT rect{};
        GetClientRect(m_Handle, &rect);
        if (!IsZoomed(m_Handle)) {
            const int bx = Border(m_Handle, SM_CXFRAME),
                      by = Border(m_Handle, SM_CYFRAME);
            const bool left = p.x < bx, right = p.x >= rect.right - bx,
                       top = p.y < by, bottom = p.y >= rect.bottom - by;
            if (top && left)
                return HTTOPLEFT;
            if (top && right)
                return HTTOPRIGHT;
            if (bottom && left)
                return HTBOTTOMLEFT;
            if (bottom && right)
                return HTBOTTOMRIGHT;
            if (left)
                return HTLEFT;
            if (right)
                return HTRIGHT;
            if (top)
                return HTTOP;
            if (bottom)
                return HTBOTTOM;
        }
        const auto hit = m_Owner.GetCaptionLayout().Hit(float(p.x), float(p.y));
        if (hit == CaptionHit::Drag)
            return HTCAPTION;
        if (hit == CaptionHit::Maximize)
            return HTMAXBUTTON;
        return HTCLIENT; // Menus, normal buttons and document fields stay
                         // interactive.
    }
    // HTMAXBUTTON enables Windows 11 Snap hover. Handle clicks against our own
    // rectangle rather than DefWindowProc's invisible native caption geometry.
    if (message == WM_NCLBUTTONDOWN && w == HTMAXBUTTON) {
        m_MaxPressed = true;
        SetCapture(m_Handle);
        return 0;
    }
    if ((message == WM_LBUTTONUP || message == WM_NCLBUTTONUP) &&
        m_MaxPressed) {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (message == WM_NCLBUTTONUP)
            ScreenToClient(m_Handle, &p);
        const bool apply = m_Owner.GetCaptionLayout().Hit(
                               float(p.x), float(p.y)) == CaptionHit::Maximize;
        m_MaxPressed = false;
        ReleaseCapture();
        if (apply)
            m_Owner.ToggleMaximize();
        return 0;
    }
    if (message == WM_KEYDOWN && w == VK_ESCAPE && m_MaxPressed) {
        m_MaxPressed = false;
        ReleaseCapture();
        return 0;
    }
    if (message == WM_CAPTURECHANGED || message == WM_CANCELMODE)
        m_MaxPressed = false;
    if (message == WM_SIZE || message == WM_DPICHANGED)
        m_Owner.SetCaptionLayout({});
    // DefWindowProc owns caption double-click, drag-to-restore,
    // Win+arrow/Alt+Space, maximize-hover Snap layouts. GLFW retains WM_CLOSE
    // dispatch, keyboard/focus, DPI, resize and maximize event bookkeeping.
    return CallWindowProcW(m_Previous, m_Handle, message, w, l);
}
} // namespace Hazel
