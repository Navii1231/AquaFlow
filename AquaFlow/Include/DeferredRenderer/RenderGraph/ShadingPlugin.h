#pragma once
#include "RenderPlugin.h"

#include "../Pipelines/ShadingPipeline.h"

AQUA_BEGIN

class ShadingPlugin : public RenderPlugin
{
public:
	ShadingPlugin() = default;
	~ShadingPlugin() = default;

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		graph[name] = CreateOp(name, EXEC_NAMESPACE::OpType::eGraphics);
		graph[name].GFX = MakeRef(mPipelineBuilder.BuildGraphicsPipeline<ShadingPipeline>(mShader, mShadingBuffer));

		graph[name].Fn = [graph](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				auto& shadingPipeline = *(ShadingPipeline*)&op.GFX;

				shadingPipeline.TraverseImageResources([cmd](const std::string& name,
					const vkLib::DescriptorLocation& location, vkLib::ImageView& view)
					{
						auto& image = *view;

						image.BeginCommands(cmd);
						image.RecordTransitionLayout(vk::ImageLayout::eGeneral);
					});

				shadingPipeline.Begin(cmd);

				shadingPipeline.Activate();
				shadingPipeline.DrawVertices(0, 0, 1, 6);

				shadingPipeline.End();

				shadingPipeline.TraverseImageResources([cmd](const std::string& name,
					const vkLib::DescriptorLocation& location, vkLib::ImageView& view)
					{
						auto& image = *view;
						image.EndCommands();
					});
			};
	}

	void SetShader(vkLib::PShader val) { mShader = val; }
	void SetFramebuffer(vkLib::Framebuffer val) { mShadingBuffer = val; }
private:
	vkLib::PShader mShader;
	vkLib::Framebuffer mShadingBuffer;
};

AQUA_END

