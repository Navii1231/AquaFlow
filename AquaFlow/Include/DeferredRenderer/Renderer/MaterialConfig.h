#pragma once
#include "../../Core/AqCore.h"
#include "../../Material/MaterialConfig.h"
#include "../../Material/MaterialBuilder.h"

AQUA_BEGIN

#define TEMPLATE_POINT            "Points"
#define TEMPLATE_LINE             "Lines"
#define TEMPLATE_PBR              "PBR"
#define TEMPLATE_BLINNPHONG       "BlinnPhong"
#define TEMPLATE_WIREFRAME        "Wireframe"

// for effective material caching
struct MaterialTemplate
{
	std::string Name;
	
	MAT_NAMESPACE::Material MatOp;
	MAT_NAMESPACE::ShaderParameterSet ParameterSet;
	DeferGFXMaterialCreateInfo CreateInfo;
};

AQUA_END
