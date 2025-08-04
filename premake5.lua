workspace "AquaFlow"
architecture "x64"
configurations { "Debug", "Release" }

startproject "Sandbox"

-- All libraries are compiled without their dependencies linked
-- Which means all of them, including vulkan and assimp dependencies, 
-- must be linked together at the final compilation (in the Sandbox project or whatever your application is)...
-- This was done to avoid the unnecessary bloating of libraries

--[[
The linked dependencies are
  **            glslang(d).lib
  **            GenericCodeGen(d).lib
  **            glslang-default-resource-limits(d).lib
  **            SPIRV(d).lib
  **            SPIRV-Tools(d).lib
  **            SPIRV-Tools-link(d).lib
  **            SPIRV-Tools-opt(d).lib
  **            spirv-cross-core(d).lib
  **            spirv-cross-glsl(d).lib
  **            OSDependent(d).lib
  **            MachineIndependent(d).lib
  **            Assimp/Debug/assimp-vc143-mt(d).lib
  **            Assimp/Debug/zlibstatic(d).lib

  Additionally, I am using Assimp dll
  **            assimp-vc143-mt(d).dll
  Place this library in the same directory where the .exe file is
]]

include "VulkanLibrary/BuildVulkanLibrary.lua"
include "AquaFlow/BuildAquaFlow.lua"
include "Sandbox/BuildSandbox.lua"
