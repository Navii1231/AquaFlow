#pragma once
#include "RenderPlugin.h"
#include "../Pipelines/DeferredPipeline.h"

AQUA_BEGIN

class GBufferPlugin : public RenderPlugin
{
public:
	GBufferPlugin() = default;
	~GBufferPlugin() = default;

	void SetShader(vkLib::PShader shader) { mShader = shader; }
	void SetGBuffer(vkLib::Framebuffer framebuffer) { mGBuffer = framebuffer; }
	void SetBindings(VertexBindingMap bindings) { mBindings = bindings; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) override
	{
		EXEC_NAMESPACE::Operation oper = CreateOp(name, EXEC_NAMESPACE::OpType::eGraphics);
		auto pipeline = mPipelineBuilder.BuildGraphicsPipeline<DeferredPipeline>(mShader, mGBuffer, mBindings);

		oper.GFX = MakeRef(pipeline);

		oper.Fn = [graph](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner exec(cmd, op);

				op.GFX->Begin(cmd);

				op.GFX->Activate();
				op.GFX->DrawIndexed(0, 0, 0, 1);

				op.GFX->End();
			};

		graph[name] = oper;
	}

private:
	VertexBindingMap mBindings;
	vkLib::Framebuffer mGBuffer;
	vkLib::PShader mShader;
};

AQUA_END
