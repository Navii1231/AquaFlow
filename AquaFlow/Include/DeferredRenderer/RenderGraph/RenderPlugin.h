#pragma once
#include "../../Execution/GraphBuilder.h"
#include "../../Execution/Executioner.h"
#include "../../Core/SharedRef.h"

AQUA_BEGIN

class RenderPlugin
{
public:
	RenderPlugin() = default;
	virtual ~RenderPlugin() = default;

	void SetPipelineBuilder(vkLib::PipelineBuilder builder) { mPipelineBuilder = builder; }
	void SetSemaphore(std::binary_semaphore* semaphore) { mSignal = semaphore; }

	virtual void AddPlugin(EXEC_NAMESPACE::GraphBuilder& graph, const std::string& name) = 0;

protected:
	vkLib::PipelineBuilder mPipelineBuilder;
	std::binary_semaphore* mSignal;

protected:
	inline EXEC_NAMESPACE::Operation CreateOp(const std::string& name, EXEC_NAMESPACE::OpType type);
};

AQUA_NAMESPACE::EXEC_NAMESPACE::Operation RenderPlugin::CreateOp(const std::string& name, EXEC_NAMESPACE::OpType type)
{
	auto op = EXEC_NAMESPACE::Operation();

	op.Name = name;
	op.States.Type = type;

	return op;
}

AQUA_END
