#pragma once
#include "Hazel/Assets/FontFamilyDocument.h"
#include "Hazel/Renderer/Framebuffer.h"
#include "Authoring/EditorActions.h"
#include <functional>
namespace Hazel {
class FontFamilyPanel {
    friend class EditorWorkflowSmoke;
public:
    void Bind(const Ref<ProjectAssets>& assets);
    bool Create(const std::filesystem::path& destination,const FontFamilyDefinition& definition);
    bool Open(const std::filesystem::path& path);
    void BeginCreate(const std::filesystem::path& folder);
    void Render(bool editable);
    bool Save();
    void Close();
    bool Dirty() const{return m_Document && m_Document->Dirty();}
    bool HasDocument() const{return bool(m_Document);}
    bool Focused() const{return m_Open && m_Focused;}
    bool Visible() const{return m_Open;}
    void Show(){if(m_Document)m_Open=true;}
    uint64_t Identity() const{return m_Identity;}
    std::string Name() const{return m_Document?m_Document->Reference().generic_u8string():"No family";}
    std::string CopyDraftText() const{return m_Document?WriteFontFamilyText(m_Document->Draft()):std::string{};}
    std::function<void()> RequestClose, AssetsChanged;
    std::function<void(std::function<bool()>)> ReplaceDocument;
    std::function<void(const std::string&)> ReportError;
    std::function<ActionAvailability(EditorAction)> Availability;
    std::function<bool()> GuardPending;
private:
    void PathField(const char* id,const char* label,std::filesystem::path& value,bool face,bool mark=true);
    void Inspect();
    void UpdatePreview();
    bool CanEdit() const;
    Ref<ProjectAssets> m_Assets;
    Scope<FontFamilyDocument> m_Document;
    std::vector<FamilyCoverage> m_Coverage;
    std::string m_Error,m_CoverageError,m_Destination,m_Regular,m_Notice,m_Preview="The quick brown fox 0123456789";
    uint64_t m_Identity=0,m_CoverageEpoch=0;
    bool m_Open=false,m_Focused=false,m_Create=false,m_ResumeCreate=false,m_CoverageStale=true;
    int m_Style=0;
    std::string m_AcceptedPreview,m_AtlasCoverage;
    Ref<Font> m_PreviewFont;
    Ref<Framebuffer> m_PreviewTarget;
};
}
