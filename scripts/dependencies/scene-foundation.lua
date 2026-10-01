-- Root-owned build settings; retain the target YAML source and its vendor scripts.
local root = _MAIN_SCRIPT_DIR
project "yaml-cpp"
    location (root .. "/build/yaml-cpp")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files { root .. "/Hazel/vendor/yaml-cpp/src/**.h", root .. "/Hazel/vendor/yaml-cpp/src/**.cpp",
            root .. "/Hazel/vendor/yaml-cpp/include/**.h" }
    includedirs { root .. "/Hazel/vendor/yaml-cpp/include" }
    -- Current gmake does not emit file-specific forceincludes. Scope the harmless
    -- standard integer declarations to this dependency project on both OSes.
    forceincludes { root .. "/scripts/dependencies/yaml-compat.h" }
    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
    filter { "system:windows", "action:vs*" }
        -- MSVC resolves relative /FI paths against the source's include search,
        -- unlike GNU Make. Use the solution's absolute root for this header.
        removeforceincludes { root .. "/scripts/dependencies/yaml-compat.h" }
        buildoptions { '/FI"$(SolutionDir)scripts/dependencies/yaml-compat.h"' }
    filter "system:linux"
        toolset "gcc"
        pic "On"
    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
    filter "configurations:Dist"
        runtime "Release"
        optimize "On"
    filter {}

local consumers = { "Hazel", "Sandbox" }
if _OPTIONS["migration-tests"] then
    for _, name in ipairs { "RendererSmoke", "CoreSmoke", "RendererFeaturesSmoke", "SceneFoundationSmoke" } do
        table.insert(consumers, "Migration" .. name)
    end
end
for _, name in ipairs(consumers) do
    project (name)
        externalincludedirs { root .. "/Hazel/vendor/entt/include", root .. "/Hazel/vendor/filewatch",
                              root .. "/Hazel/vendor/yaml-cpp/include" }
        if name ~= "Hazel" then
            links { "yaml-cpp" }
        end
        filter {}
end
