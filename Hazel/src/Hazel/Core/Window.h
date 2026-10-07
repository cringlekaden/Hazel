#pragma once

#include "Hazel/Core/Core.h"
#include "Hazel/Events/Event.h"

#include <functional>
#include <string>
#include "WindowPlacement.h"
#include "WindowCaption.h"

namespace Hazel {

    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(const std::string& title = "Hazel Engine", unsigned int width = 1280, unsigned int height = 720) : Title(title), Width(width), Height(height) {}
    };

    // Interface representing a desktop system based Window
    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        // Window attributes
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual void SetTitle(const std::string& title) = 0;
        virtual bool IsVSync() const = 0;
        
        virtual void* GetNativeWindow() const = 0;
        WindowPlacement GetPlacement();
        void RestorePlacement(const WindowPlacement& value);
        std::vector<DisplayArea> GetDisplayAreas() const;
        float GetContentScale() const;

        virtual void SetCustomCaption(bool requested);
        virtual void UseNativeCaption(const std::string& reason);
        const CaptionState& GetCaptionState() const {return m_CaptionState;}
        void SetCaptionLayout(const CaptionLayout& layout) {m_CaptionLayout=layout;}
        const CaptionLayout& GetCaptionLayout() const {return m_CaptionLayout;}
        void Minimize();
        void ToggleMaximize();
        bool IsFocused() const;
        CaptionHit GetCaptionPointerHit() const;
        virtual void RequestClose()=0;
        static Scope<Window> Create(const WindowProps& props = WindowProps());
    protected:
        CaptionState m_CaptionState;
        CaptionLayout m_CaptionLayout;
    private:
        WindowPlacement m_NormalPlacement;
    };
}
