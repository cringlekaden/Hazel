#pragma once
#include "Authoring/EditorActions.h"
#include "Hazel/Assets/SpriteSheetDocument.h"
#include <functional>
#include <unordered_map>
namespace Hazel
{
class SpriteSheetPanel
{
    friend class EditorWorkflowSmoke;

  public:
    void Bind(const Ref<ProjectAssets> &assets);
    bool Open(const std::filesystem::path &path);
    void BeginCreate(const std::filesystem::path &texture);
    void BeginImport();
    void Render(bool editable);
    void Tick(double timestep);
    bool Dirty() const
    {
        return m_Document && m_Document->Dirty();
    }
    bool Save();
    bool Discard();
    bool Focused() const
    {
        return m_Open && m_Focused;
    }
    bool HasDocument() const
    {
        return bool(m_Document);
    }
    uint64_t Identity() const
    {
        return m_Identity;
    }
    std::string Name() const
    {
        return m_Document ? m_Document->Reference().generic_u8string() : "No sheet";
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
    void Close();
    std::function<ActionAvailability(EditorAction)> Availability;
    std::function<SceneTarget()> CaptureTarget;
    std::function<std::string()> TargetName;
    std::function<bool(SpriteReference, SceneTarget)> AssignSprite;
    std::function<bool(AnimationReference, SceneTarget)> AssignClip;
    std::function<void()> RequestClose;
    std::function<void()> AssetsChanged;
    std::function<void(std::function<bool()>)> ReplaceDocument;
    std::function<bool()> GuardPending;
    std::function<void(const std::string &)> ReportError;

  private:
    void Canvas(bool editable);
    void Regions(bool editable);
    void Clips(bool editable);
    void Sampling();
    void PreviewTexture();
    void Fail(const std::exception &);
    bool CanEdit() const;
    bool Assign(bool clip, SpriteID id);
    Scope<SpriteSheetDocument> m_Document;
    Ref<ProjectAssets> m_Assets;
    Ref<Texture2D> m_Texture;
    uint64_t m_PreviewEpoch = 0;
    std::string m_Error, m_CreateTexture, m_CreateDestination;
    std::string m_ImportSource, m_ImportDestination;
    std::filesystem::path m_RecoveryPath;
    bool m_Recovery = false;
    bool m_Import = false;
    bool m_ImportCreateSheet = true, m_Imported = false;
    bool m_ResumeImport = false, m_ResumeCreate = false;
    uint64_t m_Identity = 0, m_NextFrameKey = 1;
    SpriteID m_AddFrameRegion = 0;
    std::unordered_map<SpriteID, std::vector<uint64_t>> m_FrameKeys;
    std::string m_RegionSearch, m_ClipSearch;
    bool m_Open = false, m_Focused = false, m_Create = false, m_PreviewStale = true;
    SpriteID m_Selected = 0, m_SelectedClip = 0, m_DeleteID = 0;
    bool m_DeleteClip = false, m_DeleteRequested = false;
    std::vector<std::string> m_DeleteUses;
    GridSliceOptions m_Grid;
    std::vector<SpriteRegion> m_GridPreview;
    SpritePlayback m_Playback;
    float m_Zoom = 2;
    glm::vec2 m_Pan{12, 12}, m_DragStart{};
    PixelRect m_Original;
    enum class Tool
    {
        Select,
        Create
    };
    Tool m_Tool = Tool::Select;
    int m_Gesture = 0; // 1 create, 2 move, 3 bottom-right resize, 4 pivot
    bool m_PivotTool = false;
};
} // namespace Hazel
