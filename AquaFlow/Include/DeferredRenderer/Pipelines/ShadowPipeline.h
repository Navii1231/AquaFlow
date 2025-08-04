#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

class ShadowPipeline : public vkLib::GraphicsPipeline
{
public:
	ShadowPipeline() = default;
	ShadowPipeline(vkLib::PShader shader, vkLib::Framebuffer depthBuffer, const VertexBindingMap& vertexBindings);

	virtual ~ShadowPipeline() = default;

	virtual void UpdateDescriptors();

	void SetModels(Mat4Buf mats) { mModels = mats; }
	void SetCamerasInfos(DepthCameraBuf camera) { mCameraInfos = camera; }

private:

	glm::uvec2 mScrSize;

	VertexBindingMap mVertexBindings;

	mutable DepthCameraBuf mCameraInfos;
	mutable Mat4Buf mModels; // Bound at (set: 0, binding: 1)

private:
	void SetupConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize);
};

AQUA_END
