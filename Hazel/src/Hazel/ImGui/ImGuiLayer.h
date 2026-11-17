#pragma once

#include "Hazel/Core/Layer.h"
#include "Hazel/Core/FileLease.h"
#include <utility>
#include "Hazel/Renderer/Resources/EditorTexture.h"
#include <vector>

namespace Hazel {

    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer();
        
        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& e) override;

        static void Image(const EditorTexture& texture, float width, float height);
        static void* Texture(const EditorTexture& texture);
        void* InputWindow() const;
        bool InputFocused() const;
        bool PointerCaptured() const { return m_PointerWindow != nullptr; }
        void CapturePointer(bool captured);
        void ReleasePlatformWindow(void* native);
        void Begin();
        void End();
        std::pair<float,float> CurrentWindowFramebufferScale() const;
        uint32_t GetActiveWidgetID() const;
        void SetDarkThemeColors();
        void BlockEvents(bool block) { m_BlockEvents = block; }
        bool OwnsWorkspace() const { return bool(m_SettingsLease); }
        const std::string& InstanceToken() const { return m_InstanceToken; }
    private:
        void* m_PointerWindow = nullptr;
        std::vector<EditorTexture> m_Textures;
        bool m_BlockEvents = true;
        std::string m_IniPath;
        std::string m_IniOriginal, m_InstanceToken;
        bool m_IniExisted = false;
        std::unique_ptr<FileLease> m_SettingsLease;
        bool m_GLFWInitialized = false, m_OpenGLInitialized = false;
    };
}
