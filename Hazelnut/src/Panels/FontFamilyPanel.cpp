#include "FontFamilyPanel.h"
#include "ContentBrowserPayload.h"
#include "UI/PropertyUI.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Utils/UTF8.h"
#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel {
bool FontFamilyPanel::CanEdit() const{return !Availability || bool(Availability(EditorAction::EditAsset));}
void FontFamilyPanel::Bind(const Ref<ProjectAssets>& assets){Close();m_Assets=assets;m_Create=m_ResumeCreate=false;}
void FontFamilyPanel::Close(){m_Document.reset();m_PreviewFont.reset();m_Open=m_Focused=false;m_Coverage.clear();m_Error.clear();m_CoverageError.clear();++m_Identity;}
bool FontFamilyPanel::Create(const std::filesystem::path& destination,const FontFamilyDefinition& definition) {
    if(!CanEdit())return false;
    try{auto doc=CreateScope<FontFamilyDocument>(m_Assets);doc->Create(destination,definition);m_Document=std::move(doc);m_Open=true;m_CoverageStale=true;m_Coverage.clear();m_PreviewFont.reset();m_Error.clear();m_ResumeCreate=false;if(definition.Sample.size()<=1024)m_Preview=definition.Sample;++m_Identity;if(AssetsChanged)AssetsChanged();return true;}
    catch(const std::exception& error){m_Error=error.what();if(ReportError)ReportError(m_Error);return false;}
}
bool FontFamilyPanel::Open(const std::filesystem::path& path) {
    try {
        auto candidate=CreateScope<FontFamilyDocument>(m_Assets);
        candidate->Open(Project::MakeAssetReference(m_Assets->Root(),path));
        m_Document=std::move(candidate);m_Open=true;m_Error.clear();m_Coverage.clear();m_CoverageStale=true;m_PreviewFont.reset();m_Style=0;if(m_Document->Draft().Sample.size()<=1024)m_Preview=m_Document->Draft().Sample;++m_Identity;return true;
    }catch(const std::exception& error){m_Error=error.what();if(ReportError)ReportError(m_Error);return false;}
}
void FontFamilyPanel::BeginCreate(const std::filesystem::path& folder) {
    if(!CanEdit())return;
    m_ResumeCreate=false;m_Regular.clear();m_Notice.clear();
    if(folder.extension()==".ttf" || folder.extension()==".otf") {
        auto face=Project::MakeAssetReference(m_Assets->Root(),folder);(void)Project::ResolveOwnedAsset(m_Assets->Root(),face);
        m_Regular=face.generic_u8string();face.replace_extension(".hfont");m_Destination=face.generic_u8string();
    }else m_Destination=(folder/"NewFamily.hfont").lexically_normal().generic_u8string();
    m_Error.clear();m_Create=true;
}
bool FontFamilyPanel::Save() {
    if(!CanEdit() || !m_Document)return false;
    try {m_Document->Save(Resources::Get().UserData/"recovery");m_Error.clear();m_CoverageStale=true;if(AssetsChanged)AssetsChanged();return true;}
    catch(const std::exception& error){m_Error=error.what();if(ReportError)ReportError(m_Error);return false;}
}
void FontFamilyPanel::PathField(const char* id,const char* label,std::filesystem::path& value,bool face,bool mark) {
    ImGui::PushID(id);std::string draft=value.generic_u8string();
    // InputText retains its own active buffer; publishing draft fields is harmless
    // until explicit Save, just like the existing sprite-sheet document.
    bool changed=PropertyUI::Text("path",label,draft).Changed;
    if(changed)value=std::filesystem::u8path(draft);
    if(ImGui::BeginDragDropTarget()) {
        if(auto* payload=ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))try {
            auto reference=Project::MakeAssetReference(m_Assets->Root(),ContentBrowserPath(payload->Data,payload->DataSize));(void)Project::ResolveOwnedAsset(m_Assets->Root(),reference);value=reference;changed=true;
        }catch(const std::exception& error){m_Error=error.what();}
        ImGui::EndDragDropTarget();
    }
    if(ImGui::SmallButton("Choose / import..."))try {
        const auto chosen=FileDialogs::OpenFile(face?"Font face\0*.ttf;*.otf\0":"Notice / license\0*\0");
        if(!chosen.empty()) {
            auto source=std::filesystem::u8path(chosen);std::filesystem::path reference;
            try{reference=Project::MakeAssetReference(m_Assets->Root(),source);(void)Project::ResolveOwnedAsset(m_Assets->Root(),reference);}catch(const std::exception&) {
                const auto destination=FileDialogs::SaveFile(face?"Import face inside Assets\0*.ttf;*.otf\0":"Import notice inside Assets\0*\0");
                if(!destination.empty()){reference=Project::MakeAssetReference(m_Assets->Root(),std::filesystem::u8path(destination));m_Assets->ImportFontFile(source,reference,face);if(AssetsChanged)AssetsChanged();}
            }
            if(!reference.empty()){value=reference;changed=true;}
        }
    }catch(const std::exception& error){m_Error=error.what();}
    if(changed && mark){if(m_Document)m_Document->Changed();m_CoverageStale=true;}
    ImGui::PopID();
}
void FontFamilyPanel::Inspect() {
    m_Coverage.clear();m_CoverageError.clear();
    try{m_Coverage=InspectFontFamily(m_Assets->Root(),m_Document->Draft());m_CoverageStale=false;m_CoverageEpoch=m_Assets->Epoch();}
    catch(const std::exception& error){m_CoverageError=error.what();}
}
void FontFamilyPanel::UpdatePreview() {
    try {
        if(m_Preview.size()>1024)throw std::runtime_error("Preview text exceeds 1024 bytes");
        std::vector<uint32_t> scalars;UTF8::Decode(m_Preview,scalars);
        auto selected=m_Document->Draft().Select(FontStyle(m_Style));
        auto font=Font::Load(Project::ResolveOwnedAsset(m_Assets->Root(),m_Document->Draft().Faces[unsigned(selected)].File),m_PreviewFont);
        auto layout=font->Layout(m_Preview,-1,false);auto pages=font->PreparePages(*layout);
        m_PreviewFont=std::move(font);m_AcceptedPreview=m_Preview;
        m_AtlasCoverage=std::to_string(layout->Glyphs.size())+" shaped glyphs; "+std::to_string(layout->MissingGlyphs)+" .notdef; "+std::to_string(pages.size())+" resident pages";
        m_Error=m_PreviewFont->PageError();
    }catch(const std::exception& error){m_Error=error.what();}
}
void FontFamilyPanel::Render(bool editable) {
    if(m_ResumeCreate && (!GuardPending || !GuardPending())){m_ResumeCreate=false;m_Create=true;}
    if(m_Create){ImGui::OpenPopup("Create font family");m_Create=false;}
    if(ImGui::BeginPopupModal("Create font family",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::BeginDisabled(!editable);
        PropertyUI::Text("destination","Project .hfont",m_Destination);
        auto face=std::filesystem::u8path(m_Regular),notice=std::filesystem::u8path(m_Notice);
        PathField("regular","Regular face",face,true,false);PathField("notice","Associated notice",notice,false,false);
        m_Regular=face.generic_u8string();m_Notice=notice.generic_u8string();
        ImGui::TextWrapped("Select licensed files inside Assets, or choose/import external files. A notice association does not establish redistribution permission.");
        if(ImGui::Button("Create")) {
            FontFamilyDefinition def;def.Name=std::filesystem::u8path(m_Destination).stem().generic_u8string();def.Faces[0]={face,{notice}};
            const auto destination=std::filesystem::u8path(m_Destination);
            auto create=[this,def,destination] {return Create(destination,def);};
            try {
                FontSelection{destination}.Validate();if(std::filesystem::exists(Project::ResolveOwnedAsset(m_Assets->Root(),destination)))throw std::runtime_error("Destination already exists; choose a new name or open the existing family");(void)WriteFontFamilyText(def);(void)InspectFontFamily(m_Assets->Root(),def);
                // Close before the common unsaved-document guard opens. Invalid
                // drafts keep this form and the previous document intact.
                m_ResumeCreate=true;ImGui::CloseCurrentPopup();if(ReplaceDocument)ReplaceDocument(create);else create();
            }catch(const std::exception& error){m_Error=error.what();}
        }
        ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel")){m_ResumeCreate=false;ImGui::CloseCurrentPopup();}
        PropertyUI::Validation(m_Error.c_str());ImGui::EndPopup();
    }
    if(!m_Open || !m_Document)return;
    bool open=true;
    ImGui::SetNextWindowSize({460,640},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Font Family",&open)) {
        m_Focused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ImGui::TextWrapped("%s%s",Name().c_str(),Dirty()?" *":"");
        ImGui::BeginDisabled(!editable);
        if(ImGui::Button("Save"))Save();
        ImGui::SameLine();if(ImGui::Button("Reopen / discard") && ReplaceDocument){auto path=m_Assets->Root()/m_Document->Reference();ReplaceDocument([this,path]{return Open(path);});}
        ImGui::SameLine();if(ImGui::Button("Reload saved")){m_Assets->Reload(m_Document->Reference());m_PreviewFont.reset();}
        auto& def=m_Document->Draft();bool changed=PropertyUI::Text("name","Family name",def.Name).Changed;
        for(unsigned i=0;i<4;++i) {
            ImGui::PushID(int(i));if(i==0)ImGui::SetNextItemOpen(true,ImGuiCond_Once);
            if(ImGui::TreeNode(FontStyleName(FontStyle(i)))) {
                auto& face=def.Faces[i];PathField("face","Face file",face.File,true);
                if(i && ImGui::SmallButton("Remove optional face")){face={};changed=true;}
                for(size_t j=0;j<face.Notices.size();) {
                    ImGui::PushID(int(j));PathField("notice","Notice file",face.Notices[j],false);
                    if(ImGui::SmallButton("Remove notice")){face.Notices.erase(face.Notices.begin()+j);changed=true;ImGui::PopID();}else{++j;ImGui::PopID();}
                }
                if(face.Notices.size()<8 && ImGui::SmallButton("Add associated notice")){face.Notices.emplace_back();changed=true;}
                ImGui::TreePop();
            }ImGui::PopID();
        }
        if(ImGui::TreeNode("Coverage declaration")) {
            changed|=PropertyUI::Multiline("required","Required text",def.Required).Changed;
            PropertyUI::Help("Up to 8192 UTF-8 bytes / 4096 unique scalars. Every provided style must shape this exact corpus without missing source glyphs. Unmet requirements block export. Empty declares no additional coverage.");
            changed|=PropertyUI::Multiline("sample","Sample text",def.Sample).Changed;
            if(ImGui::Button("Inspect coverage"))Inspect();
            ImGui::TextDisabled("CPU source/shaping inspection; no GPU pages generated. Preview reports accepted resident pages separately.");
            if(m_CoverageStale || m_CoverageEpoch!=m_Assets->Epoch())ImGui::TextDisabled("Results need inspection after draft or dependency changes.");
            PropertyUI::Validation(m_CoverageError.c_str());
            for(auto& report:m_Coverage){ImGui::Text("%s",FontStyleName(report.Style));ImGui::TextWrapped("Required: %s",report.Required.Summary().c_str());ImGui::TextWrapped("Sample (informational): %s",report.Sample.Summary().c_str());}
            ImGui::TextWrapped("Latin/Greek/Cyrillic/Arabic/Hebrew corpus uses shaping and bidi. Missing source glyphs show .notdef; unavailable pages show magenta metric placeholders. No cross-family fallback or all-Unicode guarantee. Newline, tab, CR and BOM follow the text contract.");
            ImGui::TreePop();
        }
        if(changed){m_Document->Changed();m_CoverageStale=true;}
        PropertyUI::Validation(m_Error.c_str());
        ImGui::EndDisabled();
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(ImGui::TreeNode("Face preview")) {
            PropertyUI::Combo("style","Requested style",m_Style,"Regular\0Bold\0Italic\0Bold Italic\0");
            PropertyUI::Text("preview","Preview text",m_Preview);
            auto selected=def.Select(FontStyle(m_Style));
            PropertyUI::ReadOnly("selected","Selected face",FontStyleName(selected));
            if(selected!=FontStyle(m_Style))ImGui::TextWrapped("Absent style uses Regular. No synthetic weight/slant.");
            if(ImGui::Button("Update preview"))UpdatePreview();
            if(m_PreviewFont)ImGui::TextWrapped("%s",m_AtlasCoverage.c_str());
            if(m_PreviewFont)try {
                if(!m_PreviewTarget){FramebufferSpecification spec;spec.Width=640;spec.Height=96;spec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};m_PreviewTarget=Framebuffer::Create(spec);}
                struct Binding{Ref<Framebuffer> Target;RasterState State;~Binding(){Target->Unbind();RenderCommand::SetRasterState(State);}} binding{m_PreviewTarget,RenderCommand::GetRasterState()};
                m_PreviewTarget->Bind();auto raster=binding.State;raster.DepthTest=false;raster.ClipEnabled=false;raster.Viewport={0,0,640,96};RenderCommand::SetRasterState(raster);
                RenderCommand::SetClearColor({.08f,.08f,.08f,1});RenderCommand::Clear();m_PreviewTarget->ClearAttachment(1,-1);
                Camera camera(glm::ortho(0.f,640.f,0.f,96.f));Renderer2D::BeginOrderedScene(camera,glm::mat4(1));
                struct Submit{~Submit(){Renderer2D::EndScene();}} submit;
                Renderer2D::DrawString(m_AcceptedPreview,m_PreviewFont,glm::translate(glm::mat4(1),{8,38,0})*glm::scale(glm::mat4(1),{28,28,1}),{{1,1,1,1},0,0},0);
            }catch(const std::exception& error){m_Error=error.what();}
            if(m_PreviewTarget && m_PreviewFont){auto width=std::min(640.f,ImGui::GetContentRegionAvail().x);ImGui::Image(ImTextureID(uint64_t(m_PreviewTarget->GetColorAttachmentRendererID())),{width,96*width/640},{0,1},{1,0});ImGui::TextWrapped("Actual preview: %s (last accepted Update)",m_PreviewFont->GetPath().filename().generic_u8string().c_str());}
            ImGui::TreePop();
        }
    }else m_Focused=false;
    ImGui::End();if(!open && RequestClose)RequestClose();
}
}
