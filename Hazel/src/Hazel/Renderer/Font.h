#pragma once
#include <filesystem>
#include <list>
#include <string>
#include <vector>
#include "Hazel/Core/Base.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Text/TextLayout.h"
#include <glm/glm.hpp>

namespace Hazel {
    // RGB8 level-zero estimates, excluding driver allocation overhead.
    struct FontLimits {
        uint64_t InputBytes=4*1024*1024, AtlasBytes=3*1024*1024;
        uint64_t CacheBytes=12*1024*1024, LiveAtlasBytes=24*1024*1024;
        uint64_t GeneratorBytes=32*1024*1024;
        uint32_t AtlasDimension=1024, CacheEntries=8, LiveAtlases=24;
    };
    struct FontResourceStats {
        uint64_t CacheBytes=0, LiveAtlasBytes=0, PinnedBytes=0, Generations=0, Hits=0, Evictions=0;
        uint64_t LayoutBuilds=0, LayoutHits=0, PageHits=0, PageFailures=0;
        uint32_t CacheEntries=0, CachePages=0, LiveAtlases=0, PinnedAtlases=0, LiveFonts=0;
        FontLimits Limits;
    };
    struct FontCoverageIssue { uint32_t Scalar=0; bool Source=false, Typography=false; };
    struct FontCoverage {
        uint32_t Scalars=0, MissingSource=0, Unsupported=0;
        std::vector<FontCoverageIssue> Issues;
        std::string Detail;
        bool Satisfied() const {return !MissingSource && !Unsupported && Detail.empty();}
        std::string Summary() const;
    };
    // Immutable after publication. CPU layouts never reference this GPU object.
    struct FontPage {
        struct Glyph { uint32_t ID; glm::vec2 Min,Max,UVMin,UVMax; bool Empty=false; };
        Ref<const Text::Face> Face;
        std::filesystem::path Path; // accepted source identity for project retirement
        ~FontPage();
        Ref<Texture2D> Texture;
        std::vector<Glyph> Glyphs; // sorted glyph IDs, at most 128
        uint64_t Bytes=0;
        const Glyph* Find(uint32_t id) const;
    };
    class Font {
    public:
        explicit Font(const std::filesystem::path& font);
        ~Font();
        const std::filesystem::path& GetPath() const {return m_Path;}
        uint64_t GetAtlasBytes() const; // live pages for this immutable face
        uint32_t PageCount() const;
        const Ref<const Text::Face>& GetCPUFace() const {return m_CPUFace;}
        const std::string& PageError() const {return m_PageError;}
        const std::string& TextWarning() const {return m_TextWarning;}
        // Font-height units. Shared results retain logical maps; page preparation
        // is a separate renderer-thread boundary and cannot change layout metrics.
        Ref<const Text::Layout> Layout(const std::string& text,float width,bool wrap,float tracking=0,float spacing=0,Text::Overflow overflow=Text::Overflow::Clip,uint32_t maxLines=0,float height=-1);
        std::vector<Ref<const FontPage>> PreparePages(const Text::Layout& layout);
        static FontCoverage InspectCoverage(const std::filesystem::path& path,const std::string& corpus);
        static void ValidateFile(const std::filesystem::path& path);
        static Ref<Font> Load(const std::filesystem::path& path, const Ref<Font>& accepted={});
        static Ref<Font> GetDefault();
        static void Initialize(const FontLimits& limits={});
        static void ReleaseRoot(const std::filesystem::path& root);
        static void Trim(); // releases reusable ownership; also explicitly retries failed pages
        static FontResourceStats Resources();
        static void Shutdown();
    private:
        struct Input;
        struct LayoutEntry { float Width,Tracking,Spacing,Height;bool Wrap;Text::Overflow OverflowMode;uint32_t MaxLines;Ref<const Text::Layout> Result; };
        explicit Font(Input& input);
        void Accept(Input& input);
        Ref<const FontPage> GeneratePage(const std::vector<uint32_t>& ids);
        Ref<const Text::Face> m_CPUFace;
        std::filesystem::path m_Path;
        std::string m_IdentityKey,m_PageError,m_TextWarning;
        std::list<LayoutEntry> m_Layouts;
        uint64_t m_LayoutBytes=0,m_PageFailureEpoch=0;
        bool m_Accounted=false;
        static Ref<Font> s_DefaultFont;
    };
}
