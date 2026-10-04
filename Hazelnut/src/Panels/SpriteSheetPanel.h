#pragma once
#include "Hazel/Assets/SpriteSheetDocument.h"
#include <functional>
namespace Hazel {
class SpriteSheetPanel {
public:
    void Bind(const Ref<ProjectAssets>& assets);
    void Open(const std::filesystem::path& path);
    void BeginCreate(const std::filesystem::path& texture);
    void BeginImport();
    void Render(bool editable);
    void Tick(double timestep);
    bool Dirty() const {return m_Document && m_Document->Dirty();}
    bool Save();
    bool Discard();
    bool Focused() const {return m_Focused;}
    std::function<void(SpriteReference)> AssignSprite;
    std::function<void(AnimationReference)> AssignClip;
    std::function<void(const std::string&)> ReportError;
private:
    void Canvas(bool editable);
    void Regions(bool editable);
    void Clips(bool editable);
    void Sampling();
    void PreviewTexture();
    void Fail(const std::exception&);
    Scope<SpriteSheetDocument> m_Document;
    Ref<ProjectAssets> m_Assets;
    Ref<Texture2D> m_Texture;
    TextureSpecification m_PreviewSpec;
    std::filesystem::path m_PreviewPath;
    std::string m_Error,m_CreateTexture,m_CreateDestination;
    std::string m_ImportSource,m_ImportDestination;
    std::filesystem::path m_RecoveryPath;
    bool m_Recovery=false;
    bool m_Import=false;
    bool m_Open=false,m_Focused=false,m_Create=false,m_PreviewStale=true;
    SpriteID m_Selected=0,m_SelectedClip=0,m_DeleteID=0;
    bool m_DeleteClip=false;
    std::vector<std::string> m_DeleteUses;
    GridSliceOptions m_Grid;
    std::vector<SpriteRegion> m_GridPreview;
    SpritePlayback m_Playback;
    float m_Zoom=2;
    glm::vec2 m_Pan{12,12},m_DragStart{};
    PixelRect m_Original;
    enum class Tool {Select,Create}; Tool m_Tool=Tool::Select;
    int m_Gesture=0; // 1 create, 2 move, 3 bottom-right resize, 4 pivot
    bool m_PivotTool=false;
};
}
