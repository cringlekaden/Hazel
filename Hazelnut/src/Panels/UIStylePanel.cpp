#include "UIStylePanel.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/UI/UIStyleApplication.h"
#include "UI/PropertyUI.h"
namespace Hazel
{
void UIStylePanel::Close()
{
    m_Document.reset();
    m_Preview.reset();
    m_Target.reset();
    m_Widgets = {};
    m_Open = m_Focused = m_Create = m_ResumeCreate = false;
    ++m_Identity;
    m_Error.clear();
}
bool UIStylePanel::Create(const std::filesystem::path &reference,
                          const UIStyleDefinition &definition)
{
    if (!CanEdit())
        return false;
    try
    {
        auto candidate = CreateScope<UIStyleDocument>(m_Assets);
        candidate->Create(reference, definition);
        m_Document = std::move(candidate);
        m_Open = true;
        m_ResumeCreate = false;
        m_Error.clear();
        m_Variant = 0;
        ++m_Identity;
        if (AssetsChanged)
            AssetsChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        m_Error = e.what();
        if (ReportError)
            ReportError(m_Error);
        return false;
    }
}
bool UIStylePanel::Open(const std::filesystem::path &file)
{
    try
    {
        auto candidate = CreateScope<UIStyleDocument>(m_Assets);
        candidate->Open(Project::MakeAssetReference(m_Assets->Root(), file));
        m_Document = std::move(candidate);
        m_Open = true;
        m_Error.clear();
        m_Preview.reset();
        m_Widgets = {};
        m_Variant = 0;
        ++m_Identity;
        return true;
    }
    catch (const std::exception &e)
    {
        m_Error = e.what();
        if (ReportError)
            ReportError(m_Error);
        return false;
    }
}
void UIStylePanel::BeginCreate(const std::filesystem::path &folder)
{
    if (!CanEdit())
        return;
    m_Destination = (folder / "NewStyle.hstyle").generic_u8string();
    m_Create = true;
    m_Error.clear();
}
bool UIStylePanel::Save()
{
    if (!CanEdit() || !m_Document)
        return false;
    try
    {
        m_Document->Save(Resources::Get().UserData / "recovery");
        m_Error.clear();
        if (AssetsChanged)
            AssetsChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        m_Error = e.what();
        if (ReportError)
            ReportError(m_Error);
        return false;
    }
}
void UIStylePanel::UpdatePreview()
{
    try
    {
        auto values = m_Document->Draft().Variants.at(m_Variant).Values;
        values.Validate();
        auto scene = CreateRef<Scene>();
        scene->SetAssets(m_Assets);
        scene->OnViewportResize(640, 300);
        auto canvas = scene->CreateEntity("Style preview");
        canvas.AddComponent<UICanvasComponent>().ReferenceSize = {640, 300};
        const char *labels[] = {"Normal", "Hover", "Pressed", "Disabled", "Focused"};
        for (int i = 0; i < 5; ++i)
        {
            auto e = scene->CreateEntity(labels[i]);
            scene->Reparent(e, canvas, TransformPolicy::KeepLocal);
            auto &r = e.AddComponent<UIRectComponent>();
            r.Offset = {12 + float(i) * 126, 16};
            r.FixedSize = {116, 54};
            ApplyUIStyle(r, values);
            auto &image = e.AddComponent<SpriteRendererComponent>();
            ApplyUIStyle(image, values);
            auto &text = e.AddComponent<TextComponent>();
            text.TextString = labels[i];
            ApplyUIStyle(text, values);
            auto &style = e.AddComponent<UITextStyleComponent>();
            ApplyUIStyle(style, values);
            style.Horizontal = style.Vertical = GameUI::Align::Center;
            auto &button = e.AddComponent<UIButtonComponent>();
            ApplyUIStyle(button, values);
            button.Colors.fill(values.States.Colors[i]);
            button.Images.fill(values.States.Images[i]);
            button.Interactable = false;
        }
        auto field = scene->CreateEntity("Text window");
        scene->Reparent(field, canvas, TransformPolicy::KeepLocal);
        auto &r = field.AddComponent<UIRectComponent>();
        r.Offset = {12, 90};
        r.FixedSize = {610, 64};
        ApplyUIStyle(r, values);
        auto &text = field.AddComponent<TextComponent>();
        text.TextString = "Editable values stay separate from appearance";
        ApplyUIStyle(text, values);
        auto &image = field.AddComponent<SpriteRendererComponent>();
        ApplyUIStyle(image, values);
        auto &style = field.AddComponent<UITextStyleComponent>();
        ApplyUIStyle(style, values);
        field.AddComponent<UIButtonComponent>().Kind = GameUI::ControlKind::TextEntry;
        auto &entry = field.AddComponent<UITextEntryComponent>();
        ApplyUIStyle(entry, values);
        entry.ReadOnly = true;
        // Swatches use the same quad/clipping/painter path; preview owns no game events.
        int i = 0;
        for (auto cue :
             {values.Focus, values.Selected, values.Editing, values.Caret, values.Selection})
        {
            auto e = scene->CreateEntity("Cue color");
            scene->Reparent(e, canvas, TransformPolicy::KeepLocal);
            auto &box = e.AddComponent<UIRectComponent>();
            box.Offset = {12 + float(i++) * 126, 186};
            box.FixedSize = {112, 24};
            e.AddComponent<SpriteRendererComponent>(cue);
        }
        scene->PrepareSprites();
        scene->ValidateUI(false);
        scene->RefreshUI();
        m_Preview = std::move(scene);
        m_Error.clear();
    }
    catch (const std::exception &e)
    {
        m_Error = e.what();
    }
}
void UIStylePanel::Render(bool editable)
{
    if (m_ResumeCreate && (!GuardPending || !GuardPending()))
    {
        m_ResumeCreate = false;
        m_Create = true;
    }
    if (m_Create)
    {
        ImGui::OpenPopup("Create UI style");
        m_Create = false;
    }
    if (ImGui::BeginPopupModal("Create UI style", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        PropertyUI::Text("destination", "Project .hstyle", m_Destination);
        ImGui::BeginDisabled(!editable);
        if (ImGui::Button("Create"))
        {
            auto reference = std::filesystem::u8path(m_Destination);
            UIStyleDefinition definition = DefaultUIStyle();
            definition.Name = reference.stem().generic_u8string();
            m_ResumeCreate = true;
            ImGui::CloseCurrentPopup();
            auto action = [this, reference, definition] { return Create(reference, definition); };
            if (ReplaceDocument)
                ReplaceDocument(action);
            else
                action();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_ResumeCreate = false;
            ImGui::CloseCurrentPopup();
        }
        PropertyUI::Validation(m_Error.c_str());
        ImGui::EndPopup();
    }
    if (!m_Open || !m_Document)
        return;
    bool open = true;
    ImGui::SetNextWindowSize({440, 620}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("UI Style", &open))
    {
        m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ImGui::TextWrapped("%s%s", Name().c_str(), Dirty() ? " *" : "");
        ImGui::BeginDisabled(!editable);
        if (ImGui::Button("Save"))
            Save();
        ImGui::SameLine();
        if (ImGui::Button("Reopen / discard") && ReplaceDocument)
        {
            auto file = m_Assets->Root() / m_Document->Reference();
            ReplaceDocument([this, file] { return Open(file); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload saved"))
            m_Assets->Reload(m_Document->Reference());
        auto &d = m_Document->Draft();
        bool changed = PropertyUI::Text("name", "Style name", d.Name).Changed;
        {
            PropertyUI::Row row("variant", "Variant");
            row.Width();
            if (ImGui::BeginCombo("##variant", d.Variants.at(m_Variant).Name.c_str()))
            {
                for (size_t i = 0; i < d.Variants.size(); ++i)
                    if (ImGui::Selectable(d.Variants[i].Name.c_str()))
                    {
                        m_Variant = i;
                        m_Widgets = {};
                    }
                ImGui::EndCombo();
            }
        }
        changed |=
            PropertyUI::Text("variant-name", "Variant name", d.Variants[m_Variant].Name).Changed;
        if (d.Variants.size() < 16 && ImGui::SmallButton("Duplicate variant"))
        {
            auto v = d.Variants[m_Variant];
            v.Name = "Variant " + std::to_string(d.Variants.size() + 1);
            d.Variants.push_back(std::move(v));
            m_Variant = d.Variants.size() - 1;
            changed = true;
        }
        ImGui::SameLine();
        if (d.Variants.size() > 1 && ImGui::SmallButton("Remove variant"))
        {
            d.Variants.erase(d.Variants.begin() + m_Variant);
            m_Variant = 0;
            changed = true;
        }
        try
        {
            changed |= StyleValuesEditor(d.Variants[m_Variant].Values, m_Widgets);
        }
        catch (const std::exception &error)
        {
            m_Error = error.what();
        }
        if (ImGui::TreeNode("Shared edits across variants"))
        {
            static int group = 0;
            std::string names;
            for (unsigned i = 0; i < unsigned(UIStyleGroup::Count); ++i)
            {
                names += UIStyleGroupName(UIStyleGroup(i));
                names.push_back('\0');
            }
            names.push_back('\0');
            PropertyUI::Combo("copy-group", "Typed group", group, names.c_str());
            if (ImGui::SmallButton("Copy this group to all variants"))
            {
                auto source = d.Variants[m_Variant].Values;
                for (auto &v : d.Variants)
                    CopyUIStyleGroups(v.Values, source, UIStyleBit(UIStyleGroup(group)));
                changed = true;
            }
            PropertyUI::Help(
                "An explicit draft edit, not inheritance. Useful for updating one family/size "
                "across control roles while retaining each role's colors and metrics.");
            ImGui::TreePop();
        }
        if (changed)
            m_Document->Changed();
        PropertyUI::Help("Flat named variants, no inheritance. Saved changes update references at "
                         "explicit reload boundaries. Draft preview is isolated; it never changes "
                         "a scene or dispatches game events.");
        ImGui::EndDisabled();
        if (ImGui::Button("Update control preview"))
            UpdatePreview();
        if (m_Preview)
            try
            {
                if (!m_Target)
                {
                    FramebufferSpecification spec;
                    spec.Width = 640;
                    spec.Height = 300;
                    spec.Attachments = {FramebufferTextureFormat::RGBA8,
                                        FramebufferTextureFormat::RED_INTEGER,
                                        FramebufferTextureFormat::Depth};
                    m_Target = Framebuffer::Create(spec);
                }
                {
                    struct Binding
                    {
                        Ref<Framebuffer> Target;
                        RasterState State;
                        ~Binding()
                        {
                            Target->Unbind();
                            RenderCommand::SetRasterState(State);
                        }
                    } binding{m_Target, RenderCommand::GetRasterState()};
                    m_Target->Bind();
                    RenderCommand::SetClearColor({.04f, .04f, .04f, 1});
                    RenderCommand::Clear();
                    m_Target->ClearAttachment(1, -1);
                    m_Camera.SetViewportSize(640, 300);
                    m_Preview->OnUpdateEditor(0, m_Camera);
                }
                float width = std::min(640.f, ImGui::GetContentRegionAvail().x);
                ImGui::Image(ImTextureID(uint64_t(m_Target->GetColorAttachmentRendererID())),
                             {width, width * 300 / 640}, {0, 1}, {1, 0});
                ImGui::TextDisabled("Last accepted draft preview; cue swatches: focus / selected / "
                                    "editing / caret / selection");
            }
            catch (const std::exception &e)
            {
                m_Error = e.what();
            }
        auto status = m_Assets->Style(m_Document->Reference());
        PropertyUI::Validation(status.Error.c_str());
        PropertyUI::Validation(m_Error.c_str());
        ImGui::Text("Metadata: %zu / 64 entries, %zu / 4 MiB estimated retained bytes",
                    m_Assets->StyleCount(), m_Assets->StyleBytes());
        ImGui::Text("Live versions: %zu / 128, %zu / 8 MiB estimated bytes",
                    m_Assets->StyleLiveVersions(), m_Assets->StyleLiveBytes());
    }
    else
        m_Focused = false;
    ImGui::End();
    if (!open && RequestClose)
        RequestClose();
}
} // namespace Hazel
