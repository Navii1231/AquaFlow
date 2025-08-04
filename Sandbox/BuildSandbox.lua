outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "Sandbox"
	location ""
	kind "ConsoleApp"
	language "C++"

	targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")
    flags {"MultiProcessorCompile"}

    defines
    {
        "WIN32",
    }

	files
	{
		"%{prj.location}/**.h",
		"%{prj.location}/**.hpp",
		"%{prj.location}/**.c",
		"%{prj.location}/**.cpp",
		"%{prj.location}/**.txt",
		"%{prj.location}/**.lua",
	}

	includedirs
	{
        -- Sandbox stuff
		"%{prj.location}/Dependencies/Include/",

        -- VulkanEngine library
        "%{prj.location}/../VulkanLibrary/Include/",
		"%{prj.location}/../VulkanLibrary/Dependencies/Include/",

        -- AquaFlow project
        "%{prj.location}/../AquaFlow/Include/",
	}

    libdirs
    {
    	"%{prj.location}/Dependencies/lib/",
    }

    links
    {
        "VulkanLibrary",
        "AquaFlow"
    }

		filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "10.0"

        defines
        {
            "_CONSOLE"
        }

        filter "configurations:Debug"
            defines 
            {
                "VK_LIB_BUILD_STATIC",
                "_DEBUG"
            }

            links
            {
                "glslangd.lib",
                "GenericCodeGend.lib",
                "glslang-default-resource-limitsd.lib",
                "SPIRVd.lib",
                "SPIRV-Toolsd.lib",
                "SPIRV-Tools-linkd.lib",
                "SPIRV-Tools-optd.lib",
                "spirv-cross-cored.lib",
                "spirv-cross-glsld.lib",
                "OSDependentd.lib",
                "MachineIndependentd.lib",
            }

            inlining "Disabled"
            symbols "On"
            staticruntime "Off"
            runtime "Debug"

        filter "configurations:Release"

            defines "NDEBUG"
            optimize "Full"
            inlining "Auto"
            staticruntime "Off"
            runtime "Release"

            links
            {
                "glslang.lib",
                "GenericCodeGen.lib",
                "glslang-default-resource-limits.lib",
                "SPIRV.lib",
                "SPIRV-Tools.lib",
                "SPIRV-Tools-link.lib",
                "SPIRV-Tools-opt.lib",
                "spirv-cross-core.lib",
                "spirv-cross-glsl.lib",
                "OSDependent.lib",
                "MachineIndependent.lib",
                --"Assimp/Release/assimp-vc143-mt.lib",
                --"Assimp/Debug/zlibstatic.lib",
            }
