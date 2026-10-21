#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Hazel/Core/Base.h"
#include "Hazel/Renderer/Texture.h"

namespace Hazel {
    struct MSDFData;
    // Atlas bytes are RGB8 level-zero estimates, excluding driver allocation overhead.
    struct FontLimits {
        uint64_t InputBytes=4*1024*1024, AtlasBytes=3*1024*1024;
        uint64_t CacheBytes=12*1024*1024, LiveAtlasBytes=24*1024*1024;
        uint64_t GeneratorBytes=32*1024*1024;
        uint32_t AtlasDimension=1024, CacheEntries=8, LiveAtlases=24;
    };
    struct FontResourceStats {
        uint64_t CacheBytes=0, LiveAtlasBytes=0, PinnedBytes=0, Generations=0, Hits=0, Evictions=0;
        uint32_t CacheEntries=0, LiveAtlases=0, PinnedAtlases=0, LiveFonts=0;
        FontLimits Limits;
    };
    struct FontCoverageIssue {
        uint32_t Scalar=0;
        bool Source=false, Atlas=false, Typography=false;
    };
    struct FontCoverage {
        uint32_t Scalars=0, MissingSource=0, MissingAtlas=0, Unsupported=0;
        std::vector<FontCoverageIssue> Issues; // first 32 unique problems, in scalar order
        bool Satisfied() const {return !MissingSource && !MissingAtlas && !Unsupported;}
        std::string Summary() const;
    };
    class Font {
    public:
        explicit Font(const std::filesystem::path& font);
        ~Font();
        const MSDFData* GetMSDFData() const {return m_Data.get();}
        Ref<Texture2D> GetAtlasTexture() const {return m_AtlasTexture;}
        const std::filesystem::path& GetPath() const {return m_Path;}
        uint64_t GetAtlasBytes() const {return m_AtlasBytes;}
        static FontCoverage InspectCoverage(const std::filesystem::path& path,const std::string& corpus);
        static void ValidateFile(const std::filesystem::path& path);
        // Load/Initialize/Trim/Shutdown and all resource owners are rendering-thread bound.
        // Load hashes bounded input at explicit asset freshness boundaries, never per draw.
        static Ref<Font> Load(const std::filesystem::path& path, const Ref<Font>& accepted={});
        static Ref<Font> GetDefault();
        static void Initialize(const FontLimits& limits={});
        static void ReleaseRoot(const std::filesystem::path& root);
        static void Trim(); // release reusable ownership, never invalidate live users
        static FontResourceStats Resources();
        static void Shutdown();
    private:
        struct Input;
        explicit Font(Input& input);
        void Generate(Input& input);
        Scope<MSDFData> m_Data;
        Ref<Texture2D> m_AtlasTexture;
        std::filesystem::path m_Path;
        std::string m_IdentityKey;
        uint64_t m_AtlasBytes=0;
        bool m_Accounted=false;
        static Ref<Font> s_DefaultFont;
    };
}
