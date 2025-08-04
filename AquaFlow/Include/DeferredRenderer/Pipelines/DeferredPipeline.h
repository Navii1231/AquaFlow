#pragma once
#include "PipelineConfig.h"
#include "../Renderable/Renderable.h"
#include "../Renderable/CopyIndices.h"
#include "../Renderable/RenderableBuilder.h"

AQUA_BEGIN

/* --> For most optimal rendering, we could
* Give each deferred renderable a vertex and index buffer instance and a compute pipeline
* When submitting a renderable to graphics pipeline, all vertices and indices can be
* copied entirely through the compute pipeline, therefore avoiding any CPU work
*/

/*
* --> There are three ways we can implement this thing
* --> We're currently utilizing the third one in this implementation
* First method: to copy the vertex attributes at each frame
*	-- more efficient for frequently removing and adding stuff
* Second method: to save the whole scene at once and then move from CPU to GPU only when the scene changes
*	-- might become inefficient when large meshes are added or removed frequently
* Third method: use compute shaders or GPU command buffers to copy data from one buffer to another
*	-- seems like a nice middle ground
*/

using DeferredRenderable = Renderable;

// TODO: volatile to future changes
class DeferredPipeline : public vkLib::GraphicsPipeline
{
public:
	DeferredPipeline() = default;
	// using default tags
	DeferredPipeline(const vkLib::PShader shader, vkLib::Framebuffer buffer, const VertexBindingMap& bindings);

	virtual void UpdateDescriptors();

	virtual void Cleanup() {}

	vkLib::Buffer<CameraInfo> GetCamera() const { return mCamera; }
	vkLib::Buffer<CameraInfo> GetCameraBuffer() const { return mCamera; }
	
	// Setters
	void SetModelMatrices(Mat4Buf mats) { mModels = mats; }
	void SetCamera(CameraBuf camera) { mCamera = camera; }

private:
	VertexBindingMap mVertexBindings;

	// Data controlled by the client, but memory and transfer is managed by us
	mutable Mat4Buf mModels; // Bound at (set: 0, binding: 1)
	mutable CameraBuf mCamera; // Bound at (set: 0, binding: 0)

private:
	// Helper functions...
	void SetupPipelineConfig(vkLib::GraphicsPipelineConfig& config, const glm::uvec2& scrSize);

	void SetupPipeline(const vkLib::PShader shader, vkLib::Framebuffer buffer);
	void MakeHollow();
};

AQUA_END

