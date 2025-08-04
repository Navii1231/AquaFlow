#pragma once
#include "RenderPlugin.h"

#include "../Pipelines/TextureVisualizer.h"
#include "Core/AqCore.h"

AQUA_BEGIN

class PostProcessPlugin : public RenderPlugin
{
public:
	PostProcessPlugin() = default;
	~PostProcessPlugin() = default;

	void SetPShader(vkLib::PShader val) { mShader = val; }
	void SetFramebuffer(vkLib::Framebuffer val) { mPostProcessingBuffer = val; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		graph[name] = CreateOp(name, EXEC_NAMESPACE::OpType::eGraphics);

		graph[name].GFX = MakeRef(mPipelineBuilder.BuildGraphicsPipeline<TextureVisualizer>(mShader, mPostProcessingBuffer));

		graph[name].Fn = [graph](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				auto& pipeline = *(TextureVisualizer*)GetRefAddr(op.GFX);
				auto image = *pipeline.GetTexture();

				image.BeginCommands(cmd);
				image.RecordTransitionLayout(vk::ImageLayout::eGeneral);

				pipeline.Begin(cmd);

				pipeline.Activate();
				pipeline.DrawVertices(0, 0, 1, 6);

				pipeline.End();

				image.EndCommands();
			};
	}

private:
	vkLib::PShader mShader;
	vkLib::Framebuffer mPostProcessingBuffer;
};

AQUA_END

