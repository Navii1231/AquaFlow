#pragma once
#include "../../Core/AqCore.h"

AQUA_BEGIN

enum class HyperSurfaceType
{
	ePoint        = 1,
	eLine         = 2,
};

class HyperSurfacePipeline : public vkLib::GraphicsPipeline
{
public:
	HyperSurfacePipeline() = default;
	~HyperSurfacePipeline() = default;

	HyperSurfacePipeline(HyperSurfaceType surfaceType, const glm::vec2& scrSize, 
		vkLib::PShader shader, vkLib::RenderTargetContext targetCtx);

private:
	vkLib::GraphicsPipelineConfig SetupConfig(HyperSurfaceType type, const glm::vec2& scrSize);
};

AQUA_END
