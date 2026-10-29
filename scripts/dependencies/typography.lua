-- Pristine pinned CPU libraries. Checked-in generated Unicode 17 tables only;
-- no upstream generators, configuration tools, system libraries or downloads.
local root = _MAIN_SCRIPT_DIR
group "Dependencies"
for _, name in ipairs { "HarfBuzz", "UniBreak", "SheenBidi" } do
    project (name)
        location (root .. "/build/" .. name)
        kind "StaticLib"
        language (name == "HarfBuzz" and "C++" or "C")
        cppdialect "C++17"
        cdialect "C11"
        staticruntime "Off"
        warnings "Off"
        visibility "Hidden" -- do not interpose GTK/Pango's separate system HarfBuzz
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        filter "system:linux"
            toolset "gcc"
            pic "On"
        filter "system:windows"
            systemversion "latest"
        filter { "system:windows", "action:vs*" }
            buildoptions { "/utf-8" }
        filter "configurations:Debug"
            runtime "Debug"
            symbols "On"
        filter { "configurations:Release or Dist" }
            runtime "Release"
            optimize "On"
        filter {}
end
project "HarfBuzz"
    files { root .. "/Hazel/vendor/harfbuzz/src/harfbuzz.cc" }
    includedirs { root .. "/Hazel/vendor/harfbuzz/src" }
    defines { "HB_NO_AAT", "HB_NO_VAR", "HB_NO_VERTICAL", "HB_NO_COLOR",
        "HB_NO_DRAW", "HB_NO_PAINT", "HB_NO_SUBSET", "HB_NO_BUFFER_SERIALIZE",
        "HB_NO_BUFFER_VERIFY", "HB_NO_GETENV", "HB_NO_MMAP", "HB_NO_OPEN",
        "HB_BUFFER_MAX_LEN_FACTOR=4", "HB_BUFFER_MAX_LEN_MIN=65536", "HB_BUFFER_MAX_LEN_DEFAULT=65536",
        "HB_BUFFER_MAX_OPS_FACTOR=64", "HB_BUFFER_MAX_OPS_MIN=4096", "HB_BUFFER_MAX_OPS_DEFAULT=1048576" }
project "UniBreak"
    local dir = root .. "/Hazel/vendor/libunibreak/src/"
    includedirs { dir }
    for _, file in ipairs { "unibreakbase", "unibreakdef", "linebreak", "linebreakdata", "linebreakdef",
                            "eastasianwidthdef", "emojidef", "graphemebreak", "wordbreak" } do
        files { dir .. file .. ".c" }
    end
project "SheenBidi"
    files { root .. "/Hazel/vendor/SheenBidi/Source/SheenBidi.c" }
    includedirs { root .. "/Hazel/vendor/SheenBidi/Headers", root .. "/Hazel/vendor/SheenBidi/Source" }
    defines { "SB_CONFIG_UNITY", "SB_CONFIG_DISABLE_SCRATCH_MEMORY" }
group ""

project "Hazel"
    externalincludedirs { root .. "/Hazel/vendor/harfbuzz/src", root .. "/Hazel/vendor/libunibreak/src",
                          root .. "/Hazel/vendor/SheenBidi/Headers" }
-- All final engine consumers use the same archives/CRT; public engine headers
-- expose no third-party types. HarfBuzz uses its OT funcs, not hb-ft or system HB.
local consumers = { "Nutella", "Hazelnut", "SpriteAssetAudit", "HazelProject" }
if _OPTIONS["migration-tests"] then
    for _, name in ipairs { "GameUIIntegrationSmoke", "RendererSmoke", "CoreSmoke", "RendererFeaturesSmoke",
        "SceneFoundationSmoke", "FontSmoke", "Renderer2DSmoke", "ProjectPhysicsSmoke", "MonoSmoke", "SceneSmoke",
        "SceneGPUSmoke", "EditorSmoke", "RuntimeSessionSmoke", "ExampleGamesSmoke", "SpriteSmoke", "TypographySmoke" } do
        table.insert(consumers, "Migration" .. name)
    end
end
for _, name in ipairs(consumers) do
    project (name)
        links { "HarfBuzz", "UniBreak", "SheenBidi" }
        filter {}
end
