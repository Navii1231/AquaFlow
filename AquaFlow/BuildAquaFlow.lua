outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "AquaFlow"
	location ""
	kind "StaticLib"
	language "C++"

	targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")
    flags {"MultiProcessorCompile"}

    pchheader "Core/aqpch.h"
    pchsource "%{prj.location}/Src/Core/aqpch.cpp"

	files
	{
		"%{prj.location}/**.h",
		"%{prj.location}/**.hpp",
		"%{prj.location}/**.c",
		"%{prj.location}/**.cpp",
        "%{prj.location}/**.txt",
        "%{prj.location}/**.glsl",
		"%{prj.location}/**.vert",
		"%{prj.location}/**.frag",
		"%{prj.location}/**.geom",
		"%{prj.location}/**.comp",
        "%{prj.location}/**.yaml",
		"%{prj.location}/**.lua",
	}

	includedirs
	{
        -- AquaFlow own stuff...
		"%{prj.location}/Dependencies/include/",
		"%{prj.location}/Include/",

        -- Vulkan library...
        "%{prj.location}/../VulkanLibrary/Include/",
		"%{prj.location}/../VulkanLibrary/Dependencies/Include/",
	}

    libdirs
    {
    	"%{prj.location}/Dependencies/lib/",
    }

    links
    {

    }

		filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "10.0"

        defines
        {
            "_CONSOLE",
            "WIN32",
        }

        filter "configurations:Debug"
            defines 
            {
                "AQUA_FLOW_BUILD_STATIC",
                "_DEBUG",
            }

            links
            {
                "Assimp/Debug/assimp-vc143-mtd.lib",
                "Assimp/Debug/zlibstaticd.lib",
            }

            inlining "Disabled"
            symbols "On"
            staticruntime "Off"
            runtime "Debug"

        filter "configurations:Release"

            links
            {
                "Assimp/Release/assimp-vc143-mt.lib",
                "Assimp/Release/zlibstatic.lib",
            }

            defines "NDEBUG"
            optimize "Full"
            inlining "Auto"
            staticruntime "Off"
            runtime "Release"
