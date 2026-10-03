-- Exact upstream Box2D source/project, with root-owned generated outputs/settings.
local root = _MAIN_SCRIPT_DIR
include (root .. "/Hazel/vendor/Box2D")
project "Box2D"
    location (root .. "/build/Box2D")
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    staticruntime "Off"
    filter "system:linux"
        toolset "gcc"
    filter { "system:windows", "action:vs*" }
        buildoptions { "/utf-8" }
    filter "configurations:Debug"
        runtime "Debug"
    filter "configurations:Release"
        runtime "Release"
    filter "configurations:Dist"
        runtime "Release"
    filter {}
project "Hazel"
    externalincludedirs { root .. "/Hazel/vendor/Box2D/include" }
project "Nutella"
    links { "Box2D" }
project "Hazelnut"
    links { "Box2D" }
if _OPTIONS["migration-tests"] then
    for _, test in ipairs { "RendererSmoke", "CoreSmoke", "RendererFeaturesSmoke", "SceneFoundationSmoke", "FontSmoke", "Renderer2DSmoke", "ProjectPhysicsSmoke", "MonoSmoke", "SceneSmoke", "SceneGPUSmoke", "EditorSmoke", "RuntimeSessionSmoke", "ExampleGamesSmoke" } do
        project ("Migration" .. test)
            externalincludedirs { root .. "/Hazel/vendor/Box2D/include" }
            links { "Box2D" }
    end
end
