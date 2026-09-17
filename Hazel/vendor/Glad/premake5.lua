project "Glad"

    location "."

    kind "StaticLib"
    language "C"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "include/glad/glad.h",
        "include/KHR/khrplatform.h",
        "src/glad.c"
    }

    includedirs
    {
        "include"
    }


    -- Windows
    filter "system:windows"

        systemversion "latest"
        staticruntime "Off"

        defines
        {
            "_CRT_SECURE_NO_WARNINGS"
        }


    filter { "system:windows", "configurations:Debug" }
        runtime "Debug"


    filter { "system:windows", "configurations:Release" }
        runtime "Release"


    filter { "system:windows", "configurations:Dist" }
        runtime "Release"


    -- Linux
    filter "system:linux"

        -- Glad is a static library linked into libHazel.so.
        pic "On"


    filter "configurations:Debug"
        symbols "On"


    filter "configurations:Release"
        optimize "On"


    filter "configurations:Dist"
        optimize "On"


    filter {}