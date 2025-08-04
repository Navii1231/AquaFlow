#include "Core/Aqpch.h"
#include "Execution/GraphBuilder.h"
#include "Execution/Graph.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::Clear()
{
	ClearDependencies();
	ClearOperations();
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::InsertDependency(const std::string& from,
	const std::string& to, vk::PipelineStageFlags stageFlags)
{
	mInputDependencies[to].push_back({ from, to, stageFlags });
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::InsertPipelineOp(
	const std::string& name, vkLib::BasicPipeline& pipeline)
{
	switch (pipeline.GetPipelineBindPoint())
	{
	case vk::PipelineBindPoint::eGraphics:
		mOperations[name].States.Type = OpType::eGraphics;
		mOperations[name].GFX = std::shared_ptr<vkLib::GraphicsPipeline>();
		*mOperations[name].GFX = *reinterpret_cast<vkLib::GraphicsPipeline*>(&pipeline);
		break;
	case vk::PipelineBindPoint::eCompute:
		mOperations[name].States.Type = OpType::eCompute;
		mOperations[name].Cmp = std::shared_ptr<vkLib::ComputePipeline>();
		*mOperations[name].Cmp = *reinterpret_cast<vkLib::ComputePipeline*>(&pipeline);
		break;
	case vk::PipelineBindPoint::eRayTracingKHR:
		mOperations[name].States.Type = OpType::eRayTracing;
		_STL_ASSERT(false, "Ray tracing pipeline is yet to implement in the vkLib");
		break;
	default:
		return;
	}
}

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::Graph, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError>
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::GenerateExecutionGraph(
	const vk::ArrayProxy<std::string>& pathEnds) const
{
	for (const auto& path : pathEnds)
	{
		if (mOperations.find(path) == mOperations.end())
			return std::unexpected(GraphError::ePathDoesntExist);
	}
	auto error = ValidateDependencyInputs();

	if (!error)
		return std::unexpected(error.error());

	std::unordered_map<std::string, std::shared_ptr<Operation>> operations;

	for (const auto& path : pathEnds)
	{
		auto error = BuildDependencySkeleton(operations, path, pathEnds);

		if (!error)
			return std::unexpected(error.error());
	}

	Graph graph;
	graph.Paths = std::vector<std::string>(pathEnds.begin(), pathEnds.end());
	graph.Nodes = operations;
	graph.Lock = std::make_shared<std::mutex>();

	auto validated = graph.Validate();

	if (!validated)
		return std::unexpected(validated.error());

	return graph;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::BuildDependencySkeleton(
	std::unordered_map<std::string, std::shared_ptr<Operation>>& opCache,
	const std::string& name, const vk::ArrayProxy<std::string>& probes) const
{
	//if (std::find(probes.begin(), probes.end(), name) != probes.end())
	//	return std::unexpected(GraphError::ePathReferencedMoreThanOnce);

	if (opCache.find(name) != opCache.end())
		return true;

	opCache[name] = CreateOperation(name);
	opCache[name]->Name = name;

	for (const auto& dependencyData : mInputDependencies.at(name))
	{
		auto error = BuildDependencySkeleton(opCache, dependencyData.From, probes);

		if (!error)
			return std::unexpected(error.error());

		Dependency dependency{};
		dependency.SetOutgoingOP(opCache[name]);
		dependency.SetWaitPoint(dependencyData.WaitingStage);
		dependency.SetIncomingOP(opCache[dependencyData.From]);

		EmplaceSemaphore(dependency);

		opCache[name]->AddInputConnection(dependency);
		opCache[dependencyData.From]->AddOutputConnection(dependency);
	}

	return true;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::EmplaceSemaphore(Dependency& connection) const
{
	connection.SetSignal(mCtx.CreateSemaphore());
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::ValidateDependencyInputs() const
{
	for (const auto& [name, op] : mOperations)
	{
		auto& list = mInputDependencies[name];

		for (auto& dependency : list)
		{
			if (name == dependency.From)
				return std::unexpected(GraphError::eDependencyUponItself);

			if (mOperations.find(dependency.From) == mOperations.end())
				return std::unexpected(GraphError::eInvalidConnection);
		}
	}

	for (const auto& [name, dependency] : mInputDependencies)
	{
		if (mOperations.find(name) == mOperations.end())
			return std::unexpected(GraphError::eInvalidConnection);
	}

	return true;
}

std::shared_ptr<AQUA_NAMESPACE::EXEC_NAMESPACE::Operation> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::GraphBuilder::CreateOperation(const std::string& name) const
{
	auto opPtr = std::make_shared<Operation>(mOperations.at(name));
	opPtr->States.TraversalState = GraphTraversalState::ePending;

	return opPtr;
}
