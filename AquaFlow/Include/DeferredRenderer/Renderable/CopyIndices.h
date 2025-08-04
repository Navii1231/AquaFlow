#pragma once
#include "Renderable.h"

AQUA_BEGIN

struct CopyIdxPipeline : public vkLib::ComputePipeline
{
    CopyIdxPipeline() = default;

    CopyIdxPipeline(vkLib::PShader copyComp)
    { this->SetShader(copyComp); }

    inline void UpdateDescriptors(vkLib::GenericBuffer dst, vkLib::GenericBuffer src);

	void operator()(vk::CommandBuffer commandBuffer, vkLib::GenericBuffer dst, vkLib::GenericBuffer src, uint32_t vertexCount);
};

AQUA_END
