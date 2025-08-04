#pragma once
#include "../../Core/AqCore.h"
#include "../../Material/MaterialConfig.h"
#include "../../Material/MaterialBuilder.h"

AQUA_BEGIN

#define TEMPLATE_POINT    "points"
#define TEMPLATE_LINE     "lines"

// for effective material caching
struct MaterialTemplate
{
	std::string Name;
	
	MAT_NAMESPACE::Material MatOp;
	MAT_NAMESPACE::ShaderParameterSet ParameterSet;
	DeferGFXMaterialCreateInfo CreateInfo;
};

AQUA_END
