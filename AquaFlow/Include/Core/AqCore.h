#pragma once
// VulkanEngine library dependency...
#include "Device/Context.h"
#include "Aqpch.h"

/*
* Other than that, AquaFlow also has Assimp dll dependency
* Particularly the Assimp Importer and Exporter to
* load and save 3D models. It must be respected at the final build
*/

#define AQUA_NAMESPACE    AquaFlow
#define AQUA_BEGIN        namespace AQUA_NAMESPACE {
#define AQUA_END          }

#define EXEC_NAMESPACE    Exec
#define EXEC_BEGIN        namespace EXEC_NAMESPACE {
#define EXEC_END          }

#define MAT_NAMESPACE     MatCore
#define MAT_BEGIN         namespace MAT_NAMESPACE {
#define MAT_END           }

#define MAKE_SHARED(type, ...)   std::make_shared<type>(__VA_ARGS__)
