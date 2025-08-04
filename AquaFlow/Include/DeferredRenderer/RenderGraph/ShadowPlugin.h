#pragma once
#include "RenderPlugin.h"

#include "../Pipelines/ShadowPipeline.h"

AQUA_BEGIN

class ShadowPlugin : public RenderPlugin
{
public:
	ShadowPlugin() = default;
	~ShadowPlugin() = default;

	void SetShader(vkLib::PShader shader) { mShader = shader; }
	void SetDepthBuffer(vkLib::Framebuffer framebuffer) { mDepthBuffer = framebuffer; }
	void SetBindings(VertexBindingMap bindings) { mVertexBindings = bindings; }
	void SetCameraOffset(uint32_t offset) { mOffset = offset; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		uint32_t offset = mOffset;

		graph[name] = CreateOp(name, EXEC_NAMESPACE::OpType::eGraphics);

		graph[name].GFX = MakeRef(mPipelineBuilder.BuildGraphicsPipeline<ShadowPipeline>(mShader, mDepthBuffer, mVertexBindings));

		graph[name].Fn = [graph, offset](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				op.GFX->Begin(cmd);

				op.GFX->Activate();

				op.GFX->SetShaderConstant("eVertex.ShaderConstants.Index_0", offset);
				op.GFX->DrawIndexed(0, 0, 0, 1);

				op.GFX->End();
			};
	}

private:
	uint32_t mOffset = 0;

	VertexBindingMap mVertexBindings;
	vkLib::PShader mShader;
	vkLib::Framebuffer mDepthBuffer;
};

AQUA_END

