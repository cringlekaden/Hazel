#pragma once
#include "Authoring/EditorActions.h"
#include "Hazel/Assets/MaterialDocument.h"
#include "UI/AssetPicker.h"
namespace Hazel
{
class MaterialPanel
{
    friend class EditorWorkflowSmoke;

  public:
    void Bind(const Ref<ProjectAssets> &assets)
    {
        Close();
        m_Assets = assets;
    }
    bool Create(const std::filesystem::path &reference, const MaterialDefinition &value);
    bool Open(const std::filesystem::path &file);
    void BeginCreate(const std::filesystem::path &folder, bool duplicate = false);
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
        return m_Document ? m_Document->Reference().generic_u8string() : "No material";
    }
    std::string CopyDraftText() const
    {
        return m_Document ? WriteMaterialText(m_Document->Draft()) : std::string{};
    }
    void Finish(bool cancel)
    {
        if (m_Document)
            m_Document->Finish(cancel);
    }
    bool UndoRedo(bool redo);
    bool CanUndo() const
    {
        return m_Document && m_Document->CanUndo();
    }
    bool CanRedo() const
    {
        return m_Document && m_Document->CanRedo();
    }
    std::function<void()> RequestClose, AssetsChanged;
    std::function<void(std::function<bool()>)> ReplaceDocument;
    std::function<void(const std::string &)> ReportError;
    std::function<ActionAvailability(EditorAction)> Availability;
    std::function<bool()> GuardPending;

  private:
    bool CanEdit() const
    {
        return !Availability || bool(Availability(EditorAction::EditAsset));
    }
    void Error(const std::exception &error);
    void Properties();
    Ref<ProjectAssets> m_Assets;
    Scope<MaterialDocument> m_Document;
    std::array<AssetPickerState, 5> m_Pickers;
    std::array<Ref<Texture2D>, 5> m_Previews;
    MaterialDefinition m_CreateValues;
    std::string m_Destination, m_Error;
    uint64_t m_Identity = 0;
    bool m_Open = false, m_Focused = false, m_Create = false;
};
} // namespace Hazel
