if os.target() == "linux" then _OPTIONS["dotnet"] = "mono" end
workspace "ExampleGameTests"
    architecture "x64"
    configurations { "Debug", "Release" }
project "ExampleGameTests"
    kind "ConsoleApp"
    language "C#"
    dotnetframework "4.7.2"
    targetdir (assert(os.getenv("HAZEL_GAME_TEST_OUTPUT")))
    objdir (os.getenv("HAZEL_GAME_TEST_OUTPUT") .. "/Intermediates")
    files { "FlightTests.cs", "../../examples/Skybound/Assets/Scripts/Source/Flight.cs" }
    links { "System" }
    filter "system:linux"
        buildoptions { "-sdk:4.7.2" }
    filter "configurations:Debug"
        symbols "Default"
    filter "configurations:Release"
        optimize "On"
    filter {}
