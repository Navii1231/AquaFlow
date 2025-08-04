#pragma once
#include "PipelineConfig.h"

AQUA_BEGIN

struct SkyboxVertex
{
	glm::vec3 Vertex;
};

class SkyboxPipeline : public vkLib::GraphicsPipeline
{
public:
	SkyboxPipeline() = default;

	SkyboxPipeline(vkLib::PShader shader, vkLib::Framebuffer framebuffer);

	virtual ~SkyboxPipeline() = default;

	void SetCamera(vkLib::Buffer<CameraInfo> camera) { mCamera = camera; }
	void SetEnvironmentTexture(vkLib::Image image, vkLib::Core::Ref<vk::Sampler> sampler);
	vkLib::Image GetEnvironmentTexture() { return mEnvironmentTexture; }

	virtual void UpdateDescriptors();

private:
	vkLib::Buffer<CameraInfo> mCamera;
	vkLib::Image mEnvironmentTexture;
	vkLib::Core::Ref<vk::Sampler> mSampler;

private:
	virtual size_t GetIndexCount() const override { return 6; }

	vkLib::GraphicsPipelineConfig SetupGraphicsConfig(vkLib::RenderTargetContext ctx);
};

AQUA_END
