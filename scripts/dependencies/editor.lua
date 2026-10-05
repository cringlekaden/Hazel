-- Keep the exact target ImGuizmo pin and source clean; official ImGui remains.
local root = _MAIN_SCRIPT_DIR
project "ImGuizmo"
    location (root .. "/build/ImGuizmo")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files { root .. "/Hazel/vendor/ImGuizmo/ImGuizmo.h", root .. "/Hazel/vendor/ImGuizmo/ImGuizmo.cpp" }
    includedirs { root .. "/Hazel/vendor/imgui" }
    defines { "IMGUI_DEFINE_MATH_OPERATORS" }
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
if _OPTIONS["migration-tests"] then
    project "MigrationEditorSmoke"
        links { "ImGuizmo" }
        includedirs { root .. "/Hazelnut/src" }
        externalincludedirs { root .. "/Hazel/vendor/ImGuizmo" }
        files { root .. "/Hazelnut/src/EditorLayer.cpp", root .. "/Hazelnut/src/Panels/**.cpp", root .. "/Hazelnut/src/Authoring/**.cpp", root .. "/Hazelnut/src/UI/**.cpp" }
        dependson { "Hazel-ScriptCore", "MigrationManagedFixture" }
end
