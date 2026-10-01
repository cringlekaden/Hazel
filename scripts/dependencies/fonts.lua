-- Actual target font dependency projects; root-owned outputs and CRT settings.
local root = _MAIN_SCRIPT_DIR
include (root .. "/Hazel/vendor/msdf-atlas-gen")
for _, name in ipairs { "freetype", "msdfgen", "msdf-atlas-gen" } do
    project (name)
        location (root .. "/build/" .. name)
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        staticruntime "Off"
        filter "system:linux"
            toolset "gcc"
        filter { "system:windows", "action:vs*" }
            buildoptions { "/utf-8" }
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
end
-- Retain separate static archives; final applications resolve each dependency.
project "msdfgen"
    removelinks { "freetype" }
    dependson { "freetype" }
project "msdf-atlas-gen"
    removelinks { "msdfgen" }
    dependson { "msdfgen" }

local consumers = { "Hazel", "Sandbox" }
if _OPTIONS["migration-tests"] then
    for _, name in ipairs { "RendererSmoke", "CoreSmoke", "RendererFeaturesSmoke", "SceneFoundationSmoke", "FontSmoke", "Renderer2DSmoke" } do
        table.insert(consumers, "Migration" .. name)
    end
end
for _, name in ipairs(consumers) do
    project (name)
        externalincludedirs { root .. "/Hazel/vendor/msdf-atlas-gen/msdf-atlas-gen",
                              root .. "/Hazel/vendor/msdf-atlas-gen/msdfgen" }
        defines { "MSDFGEN_USE_CPP11" }
        if name ~= "Hazel" then
            links { "msdf-atlas-gen", "msdfgen", "freetype" }
        end
        filter {}
end
