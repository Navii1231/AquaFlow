#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/ShadowPipeline.h"

AQUA_NAMESPACE::ShadowPipeline::ShadowPipeline(vkLib::PShader shader, vkLib::Framebuffer depthBuffer, const VertexBindingMap& vertexBindings)
	: mVertexBindings(vertexBindings)
{
	this->SetShader(shader);

	vkLib::GraphicsPipelineConfig config{};

	SetupConfig(config, depthBuffer.GetResolution());

	config.TargetContext = depthBuffer.GetParentContext();

	this->SetConfig(config);

	this->SetFramebuffer(depthBuffer);
}

void AQUA_NAMESPACE::ShadowPipeline::UpdateDescriptors()
{
	// For now, we only have camera and model matrices

	vkLib::StorageBufferWriteInfo cameraInfos{};
	cameraInfos.Buffer = mCameraInfos.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 0, 0 }, cameraInfos);

	vkLib::StorageBufferWriteInfo storageInfo{};
	storageInfo.Buffer = mModels.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 1, 0 }, storageInfo);
}

void AQUA_NAMESPACE::ShadowPipeline::SetupConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize)
{
	config.CanvasScissor.offset = vk::Offset2D(0, 0);
	config.CanvasScissor.extent = vk::Extent2D(scrSize.x, scrSize.y);

	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)scrSize.x, (float)scrSize.y, 0.0f, 1.0f);

	config.Topology = vk::PrimitiveTopology::eTriangleList;

	config.IndicesType = vk::IndexType::eUint32;
	config.VertexInput = VertexFactory::GenerateVertexInputStreamInfo(mVertexBindings);

	config.SubpassIndex = 0;

	config.DepthBufferingState.DepthBoundsTestEnable = false;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthTestEnable = true;
	config.DepthBufferingState.DepthWriteEnable = true;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.StencilTestEnable = false;

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eCounterClockwise;
	config.Rasterizer.LineWidth = 0.01f;
	config.Rasterizer.PolygonMode = vk::PolygonMode::eFill;

	config.DynamicStates.emplace_back(vk::DynamicState::eScissor);
	config.DynamicStates.emplace_back(vk::DynamicState::eViewport);
	config.DynamicStates.emplace_back(vk::DynamicState::eLineWidth);
	config.DynamicStates.emplace_back(vk::DynamicState::eDepthCompareOp);
}
