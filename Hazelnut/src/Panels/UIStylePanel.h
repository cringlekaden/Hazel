#pragma once
#include "Authoring/EditorActions.h"
#include "Hazel/Assets/UIStyleDocument.h"
#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Renderer/Framebuffer.h"
#include "UI/StyleWidgets.h"
namespace Hazel
{
class Scene;
class UIStylePanel
{
    friend class EditorWorkflowSmoke;

  public:
    void Bind(const Ref<ProjectAssets> &assets)
    {
        Close();
        m_Assets = assets;
    }
    bool Create(const std::filesystem::path &reference, const UIStyleDefinition &definition);
    bool Open(const std::filesystem::path &file);
    void BeginCreate(const std::filesystem::path &folder);
    bool Save();
    void Close();
    void Render(bool editable);
    bool Dirty() const
    {
        return m_Document && m_Document->Dirty();
    }
    bool HasDocument() const
    {
        return bool(m_Document);
    }
    bool Focused() const
    {
        return m_Open && m_Focused;
    }
    bool Visible() const
    {
        return m_Open;
    }
    void Show()
    {
        if (m_Document)
            m_Open = true;
    }
    uint64_t Identity() const
    {
        return m_Identity;
    }
    std::string Name() const
    {
        return m_Document ? m_Document->Reference().generic_u8string() : "No style";
    }
    std::string CopyDraftText() const
    {
        return m_Document ? WriteUIStyleText(m_Document->Draft()) : std::string{};
    }
    std::function<void()> RequestClose, AssetsChanged;
    std::function<void(std::function<bool()>)> ReplaceDocument;
    std::function<void(const std::string &)> ReportError;
    std::function<ActionAvailability(EditorAction)> Availability;
    std::function<bool()> GuardPending;

  private:
    void UpdatePreview();
    bool CanEdit() const
    {
        return !Availability || bool(Availability(EditorAction::EditAsset));
    }
    Ref<ProjectAssets> m_Assets;
    Scope<UIStyleDocument> m_Document;
    Ref<Scene> m_Preview;
    Ref<Framebuffer> m_Target;
    EditorCamera m_Camera{30.f, 1.f, .1f, 1000.f};
    StyleWidgetState m_Widgets;
    bool m_Open = false, m_Focused = false, m_Create = false, m_ResumeCreate = false;
    size_t m_Variant = 0;
    uint64_t m_Identity = 0;
    std::string m_Destination, m_Error;
};
} // namespace Hazel
