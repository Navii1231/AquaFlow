#pragma once
#include "RenderPlugin.h"

#include "../Pipelines/SkyboxPipeline.h"

AQUA_BEGIN

class SkyboxPlugin : public RenderPlugin
{
public:
	SkyboxPlugin() = default;
	~SkyboxPlugin() = default;

	void SetPShader(vkLib::PShader val) { mShader = val; }
	void SetFramebuffer(vkLib::Framebuffer val) { mPostProcessingBuffer = val; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		graph[name] = CreateOp(name, EXEC_NAMESPACE::OpType::eGraphics);

		graph[name].GFX = MakeRef(mPipelineBuilder.BuildGraphicsPipeline<SkyboxPipeline>(mShader, mPostProcessingBuffer));

		graph[name].Fn = [graph](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				auto& pipeline = *GetRefAddr(op.GFX);

				pipeline.Begin(cmd);

				pipeline.Activate();
				pipeline.DrawVertices(0, 0, 1, 6);

				pipeline.End();
			};
	}

private:
	vkLib::PShader mShader;
	vkLib::Framebuffer mPostProcessingBuffer;
};

AQUA_END

