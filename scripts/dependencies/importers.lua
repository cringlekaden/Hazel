-- Pristine producer-only C dependencies; no parser is linked into Hazel/Nutella.
local root = _MAIN_SCRIPT_DIR
for _, item in ipairs {
    {"TinyGLTF3", "tinygltf3/tiny_gltf_v3.c", "TinyGLTFAllocation.h"},
    {"MikkTSpace", "mikktspace/mikktspace.c", "MikkAllocation.h"}
} do
    project (item[1])
        location (root .. "/build/" .. item[1])
        kind "StaticLib"
        language "C"
        cdialect "C11"
        staticruntime "Off"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/Hazel/vendor/" .. item[2] }
        includedirs { root .. "/tools/HazelImport" }
        forceincludes { root .. "/tools/HazelImport/" .. item[3] }
        if item[1] == "TinyGLTF3" then
            defines { "TINYGLTF3_MAX_MEMORY_BYTES=67108864", "TINYGLTF3_MAX_NESTING_DEPTH=64", "TINYGLTF3_MAX_STRING_LENGTH=1048576" }
        end
        filter {"system:windows", "action:vs*"}
            removeforceincludes { root .. "/tools/HazelImport/" .. item[3] }
            buildoptions { '/FI"$(SolutionDir)tools/HazelImport/' .. item[3] .. '"', "/utf-8" }
        filter "system:linux"
            pic "On"
        filter "configurations:Debug"
            runtime "Debug"
            symbols "On"
        filter "configurations:Release or Dist"
            runtime "Release"
            optimize "On"
        filter {}
end
if _OPTIONS["migration-tests"] then
    project "MigrationImportDependencySmoke"
        location (root .. "/build/MigrationImportDependencySmoke")
        kind "ConsoleApp"
        language "C"
        cdialect "C11"
        staticruntime "Off"
        targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
        files { root .. "/tests/migration/ImportDependencySmoke.c", root .. "/tools/HazelImport/Allocation.c" }
        includedirs { root .. "/Hazel/vendor/tinygltf3", root .. "/Hazel/vendor/mikktspace", root .. "/tools/HazelImport" }
        links { "TinyGLTF3", "MikkTSpace" }
        filter "system:linux"
            links { "m" }
        filter "configurations:Debug"
            runtime "Debug"
            symbols "On"
        filter "configurations:Release or Dist"
            runtime "Release"
            optimize "On"
        filter {}
end

-- Engine-owned producer module. Only native authoring helper and CPU import
-- qualification link this; Hazel/Nutella retain source/parser-free asset codecs.
local tinyPin=json.decode(io.readfile(root .. "/Hazel/vendor/tinygltf3/PROVENANCE.json"))
local mikkPin=json.decode(io.readfile(root .. "/Hazel/vendor/mikktspace/PROVENANCE.json"))
project "HazelImport"
    location (root .. "/build/HazelImport")
    kind "StaticLib"
    language "C++"
    defines {'HZ_IMPORT_TINYGLTF_COMMIT="' .. tinyPin.commit .. '"','HZ_IMPORT_MIKK_COMMIT="' .. mikkPin.commit .. '"'}
    cppdialect "C++17"
    cdialect "C11"
    staticruntime "Off"
    targetdir (root .. "/bin/" .. outputdir .. "/%{prj.name}")
    objdir (root .. "/bin-int/" .. outputdir .. "/%{prj.name}")
    files {root .. "/tools/HazelImport/**.cpp",root .. "/tools/HazelImport/Allocation.c",root .. "/tools/HazelImport/**.h"}
    includedirs {root .. "/Hazel/src",root .. "/tools/HazelImport"}
    externalincludedirs {root .. "/Hazel/vendor/spdlog/include",root .. "/Hazel/vendor/yaml-cpp/include",root .. "/Hazel/vendor/entt/include",root .. "/Hazel/vendor/imgui",root .. "/Hazel/vendor/glm",root .. "/Hazel/vendor/tinygltf3",root .. "/Hazel/vendor/mikktspace"}
    filter "system:windows"
        defines {"HZ_PLATFORM_WINDOWS"}
        buildoptions {"/utf-8"}
    filter "system:linux"
        defines {"HZ_PLATFORM_LINUX"}
        pic "On"
    filter "system:macosx"
        defines {"HZ_PLATFORM_MACOS"}
    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
    filter "configurations:Release or Dist"
        runtime "Release"
        optimize "On"
    filter {}
local importConsumers={"HazelProject"}
if _OPTIONS["migration-tests"] then table.insert(importConsumers,"MigrationSceneFoundationSmoke") end
for _, name in ipairs(importConsumers) do
    project (name)
        includedirs {root .. "/tools/HazelImport"}
        filter {}
end
