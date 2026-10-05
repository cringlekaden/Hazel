#pragma once
#include "Hazel/Core/Base.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Renderer/Texture.h"
#include <glm/glm.hpp>
#include <array>
#include <variant>
#include <string>
#include <vector>
#include <filesystem>
namespace YAML { class Node; class Emitter; }

namespace Hazel {
using SpriteID = uint64_t;
std::string SpriteIDText(SpriteID id);
SpriteID ParseSpriteID(const std::string& text);
struct PixelRect {
    uint32_t X=0,Y=0,Width=1,Height=1;
    bool operator==(const PixelRect& r) const { return X==r.X && Y==r.Y && Width==r.Width && Height==r.Height; }
};
struct SpriteRegion { SpriteID ID=0; std::string Name; PixelRect Rect; glm::vec2 Pivot{.5f,.5f}; };
struct SpriteFrame { SpriteID Region=0; double Duration=.1; };
struct SpriteClip { SpriteID ID=0; std::string Name; bool Loop=true; std::vector<SpriteFrame> Frames; };
struct SpriteSheetDefinition {
    std::filesystem::path Texture;
    TextureSpecification Sampling;
    std::vector<SpriteRegion> Regions;
    std::vector<SpriteClip> Clips;
    std::vector<SpriteID> RetiredRegionIDs,RetiredClipIDs;
    SpriteSheetDefinition();
    const SpriteRegion& Region(SpriteID id) const;
    const SpriteClip& Clip(SpriteID id) const;
    SpriteID NewID(bool clip=false) const;
    void Validate(bool playable=false) const;
};
struct SpriteReference {
    std::filesystem::path Sheet; SpriteID Region=0;
    bool operator==(const SpriteReference& b) const { return Sheet==b.Sheet && Region==b.Region; }
};
struct AnimationReference {
    std::filesystem::path Sheet; SpriteID Clip=0;
    bool operator==(const AnimationReference& b) const { return Sheet==b.Sheet && Clip==b.Clip; }
};
struct SpriteAnimationSettings {AnimationReference DefaultClip;bool Autoplay=true;double Speed=1;};
struct TextureSpriteSource {
    std::filesystem::path Texture; float TilingFactor=1;
    Ref<Texture2D> Resource; // optional native resource; generated resources without paths cannot serialize
    bool operator==(const TextureSpriteSource& b) const { return Texture==b.Texture && TilingFactor==b.TilingFactor && Resource==b.Resource; }
};
using SpriteSource=std::variant<std::monostate,TextureSpriteSource,SpriteReference>;
struct GridSliceOptions {
    uint32_t CellWidth=16,CellHeight=16,Left=0,Top=0,Right=0,Bottom=0,SpacingX=0,SpacingY=0,Columns=0,Rows=0,Start=0;
    std::string Prefix="sprite";
};
std::vector<SpriteRegion> GenerateGridPreview(const SpriteSheetDefinition&,const GridSliceOptions&);
size_t AddGridRegions(SpriteSheetDefinition&,const std::vector<SpriteRegion>&);
std::array<glm::vec2,4> SpriteUV(const PixelRect&,uint32_t width,uint32_t height);
std::array<glm::vec2,4> SpriteCorners(glm::vec2 pivot);
// Native authoring/runtime persistence. No GPU, ImGui, Mono or Python.
SpriteSheetDefinition ReadSpriteSheet(const std::filesystem::path& file);
SpriteSheetDefinition ReadSpriteSheetText(const std::string& text, bool* usesDefaults = nullptr);
std::string WriteSpriteSheetText(const SpriteSheetDefinition&);
std::string ValidateSpriteSheetSave(const std::filesystem::path& root,const std::filesystem::path& reference,const SpriteSheetDefinition&);
void SaveSpriteSheet(const std::filesystem::path& root,const std::filesystem::path& reference,const SpriteSheetDefinition&,WriteMode mode=WriteMode::Replace);
SpriteSource ReadSpriteSource(const YAML::Node& component);
void WriteSpriteSource(YAML::Emitter& out,const SpriteSource& source);
SpriteReference ReadSpriteReference(const YAML::Node&);
AnimationReference ReadAnimationReference(const YAML::Node&);
void WriteSpriteReference(YAML::Emitter&,const SpriteReference&);
void WriteAnimationReference(YAML::Emitter&,const AnimationReference&);
SpriteAnimationSettings ReadSpriteAnimationSettings(const YAML::Node&);
void WriteSpriteAnimationSettings(YAML::Emitter&,const SpriteAnimationSettings&);
// Per-owner timing value. Never stored in asset metadata or scene serialization.
struct SpritePlayback {
    double Time=0; size_t Frame=0; bool Playing=false,Finished=false;
    void Reset(bool play=false) { Time=0; Frame=0; Playing=play; Finished=false; }
    void Scrub(const SpriteClip&,double time);
    void Advance(const SpriteClip&,double timestep,double speed=1);
    static double Duration(const SpriteClip&);
};
}
