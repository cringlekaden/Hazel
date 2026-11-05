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
