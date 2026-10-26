#pragma once
#include "Hazel/Core/Base.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Hazel::Text {
class ResourcePressure : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// CPU only. All methods and final owners are bound to their service's thread.
// No font cache: accepted Font versions and caller-owned results pin these data.
struct Limits {
    uint32_t InputBytes=65536, Scalars=16384, Runs=4096, Glyphs=65536, Lines=4096;
    uint32_t ShapeScalars=1048576, ShapeCalls=32768;
    uint32_t Faces=24, Results=8192; // two 4096-element UI generations, subject to byte budget
    uint64_t FaceBytes=32*1024*1024, ResultBytes=16*1024*1024, BidiBytes=8*1024*1024;
};
struct Resources { uint32_t Faces=0, Results=0; uint64_t FaceBytes=0, ResultBytes=0; };
struct Budget;
class Service;
class Face {
public:
    ~Face();
    const std::string& Identity() const { return m_Identity; }
    uint32_t UnitsPerEm() const;
    uint32_t GlyphCount() const;
    const std::vector<uint8_t>& Bytes() const; // immutable accepted input, never a reopened path
    float HeightEm() const;
    float AscenderEm() const;
    float LineHeightEm() const;
    bool HasScalar(uint32_t scalar) const;
    bool HasInk(uint32_t glyphID) const;
private:
    friend class Service;
    struct Data;
    Scope<Data> m_Data;
    Ref<Budget> m_Budget;
    std::string m_Identity;
    Face();
};
// Boundary i is before scalar i; terminal entry maps the end. UTF-16 offsets
// count original C# code units, including an ignored leading BOM. Interior
// byte/UTF-16 surrogate positions are deliberately not scalar boundaries.
struct SourceBoundary {
    uint32_t Byte=0, UTF16=0;
    bool Grapheme=false, Line=false, Mandatory=false;
};
struct Glyph {
    uint32_t ID=0, ClusterBegin=0, ClusterEnd=0; // scalar range, not glyph count
    float X=0, Y=0, AdvanceX=0, AdvanceY=0, OffsetX=0, OffsetY=0;
    bool Generated=false; // marker cluster has no authored source: both cluster offsets are UINT32_MAX
};
struct Run {
    uint32_t Begin=0, End=0, FirstGlyph=0, GlyphCount=0, Script=0;
    uint8_t Level=0; // odd=RTL; runs/glyphs are stored in visual left-to-right order
    float X=0, Width=0;
    bool ContainsGenerated=false;
};
struct Line {
    uint32_t Begin=0, End=0, FirstRun=0, RunCount=0;
    float Width=0, Baseline=0;
    bool Overflow=false, EmergencyBreak=false;
    bool Ellipsized=false;
};
struct Paragraph { uint32_t Begin=0, End=0, SeparatorEnd=0; uint8_t Level=0; };
class Layout {
public:
    Layout()=default;
    Layout(const Layout&)=delete;
    Layout& operator=(const Layout&)=delete;
    ~Layout();
    std::string Source;
    std::vector<uint32_t> Scalars;
    std::vector<SourceBoundary> Boundaries;
    std::vector<Paragraph> Paragraphs;
    std::vector<Line> Lines;
    std::vector<Run> Runs;
    std::vector<Glyph> Glyphs;
    Ref<const Face> Font;
    float Width=0, Height=0, EmSize=1;
    uint32_t ShapeCalls=0, ShapeScalars=0, MissingGlyphs=0, MissingMarkerGlyphs=0;
    bool Omitted=false, MarkerDoesNotFit=false, ConservativeTruncation=false;
    uint32_t OriginalLines=0;
    uint64_t RetainedBytes() const { return m_Bytes; }
private:
    friend class Service;
    Ref<Budget> m_Budget;
    uint64_t m_Bytes=0;
};
enum class Direction { Auto, LTR, RTL };
enum class Overflow { Clip, Ellipsis };
struct Options {
    float EmSize=1; // logical units per em; +X right, +Y down; offsets same units
    std::optional<float> Width; // absent = unwrapped; zero is a definite constraint
    std::optional<float> Height; // full font-height line boxes; Clip retains pixel-clipped lines
    bool Wrap=true; // Width also constrains unwrapped Ellipsis
    Overflow OverflowMode=Overflow::Clip;
    uint32_t MaxLines=0; // zero = unlimited within Limits::Lines
    float Tracking=0, LineSpacing=0; // logical units; tracking between complete clusters
    bool PermitNotdef=false; // renderer reports missing source coverage, never cross-face substitution
    Direction BaseDirection=Direction::Auto;
    std::string Language="und"; // tested shaping tags: und/en/fr/de/el/ru/ar/he
};
class Service {
public:
    explicit Service(const Limits& limits={});
    // Renderer/main thread's CPU service. Does not create or retain GPU objects.
    static Service& Main();
    Ref<const Face> CreateFace(const std::vector<uint8_t>& bytes, std::string identity);
    Ref<const Layout> Arrange(std::string_view text, const Ref<const Face>& face, const Options& options={});
    Resources Usage() const;
    Service(const Service&)=delete;
    Service& operator=(const Service&)=delete;
private:
    Ref<Layout> Build(std::string_view text,const Ref<const Face>& face,const Options& options, uint32_t& calls,uint32_t& scalars);
    void CheckThread() const;
    Limits m_Limits;
    Ref<Budget> m_Budget;
    std::thread::id m_Owner;
};
}
