#include "Core/Aqpch.h"
#include "Execution/Graph.h"

void AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::operator()(vk::CommandBuffer cmds, vkLib::Core::Executor executor, std::binary_semaphore* signal) const
{
	States.Exec = State::eExecute;

	Fn(cmds, *this);
	Execute(cmds, executor, signal);

	States.Exec = State::eReady;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::operator()(vk::CommandBuffer cmds, vkLib::Core::Ref<vkLib::Core::Queue> worker, std::binary_semaphore* signal) const
{
	States.Exec = State::eExecute;

	Fn(cmds, *this);
	Execute(cmds, worker, signal);

	States.Exec = State::eReady;
}

std::expected<const vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::GetBasicPipeline() const
{
	switch (GetOpType())
	{
	case OpType::eCompute:
		return Cmp.get();
	case OpType::eGraphics:
		return GFX.get();
	case OpType::eRayTracing:
		return std::unexpected(GetOpType());
	default:
		return std::unexpected(GetOpType());
	}
}

std::expected<vkLib::BasicPipeline*, AQUA_NAMESPACE::EXEC_NAMESPACE::OpType> AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::GetBasicPipeline()
{
	switch (GetOpType())
	{
	case OpType::eCompute:
		return Cmp.get();
	case OpType::eGraphics:
		return GFX.get();
	case OpType::eRayTracing:
		return std::unexpected(GetOpType());
	default:
		return std::unexpected(GetOpType());
	}
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::Execute(vk::CommandBuffer cmd, vkLib::Core::Executor workers, 
	std::binary_semaphore* signal) const
{
	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmd, waitingList, signalList, pipelineStages);
	workers.SubmitWork(submitInfo, signal);

	return true;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::Execute(vk::CommandBuffer cmd, 
	vkLib::Core::Ref<vkLib::Core::Queue> worker, std::binary_semaphore* signal /*= nullptr*/) const
{
	SemaphoreList waitingList, signalList;
	PipelineStageList pipelineStages;

	vk::SubmitInfo submitInfo = SetupSubmitInfo(cmd, waitingList, signalList, pipelineStages);
	worker->Submit(submitInfo, signal);

	return true;
}

vk::SubmitInfo AQUA_NAMESPACE::EXEC_NAMESPACE::Operation::SetupSubmitInfo(vk::CommandBuffer& cmd,
	SemaphoreList& waitingList, SemaphoreList& signalList, PipelineStageList& pipelineStages) const
{
	for (const auto& input : InputConnections)
	{
		pipelineStages.emplace_back(input.WaitPoint);
		waitingList.emplace_back(*input.Signal);
	}

	for (const auto& input : InputInjections)
	{
		pipelineStages.emplace_back(input.WaitPoint);
		waitingList.emplace_back(*input.Signal);
	}

	for (const auto& output : OutputConnections)
	{
		signalList.emplace_back(*output.Signal);
	}

	for (const auto& output : OutputInjections)
	{
		signalList.emplace_back(*output.Signal);
	}

	vk::SubmitInfo submitInfo{};

	submitInfo.setCommandBuffers(cmd);
	submitInfo.setSignalSemaphores(signalList);
	submitInfo.setWaitSemaphores(waitingList);
	submitInfo.setWaitDstStageMask(pipelineStages);

	return submitInfo;
}

bool AQUA_NAMESPACE::EXEC_NAMESPACE::FindClosedCircuit(std::shared_ptr<Operation> node)
{
	if (node->States.TraversalState == GraphTraversalState::eVisited)
		return false;

	node->States.TraversalState = GraphTraversalState::eVisiting;

	for (const auto& input : node->InputConnections)
	{
		// return if we find a closed circuit
		if (input.Incoming->States.TraversalState == GraphTraversalState::eVisiting)
			return true;

		if (FindClosedCircuit(input.Incoming))
			return true;
	}

	node->States.TraversalState = GraphTraversalState::eVisited;

	return false;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::InsertNode(GraphList& list, std::shared_ptr<Operation> node)
{
	// if the node is already visited, we exit
	if (node->States.TraversalState == GraphTraversalState::eVisited)
		return;

	// visit all incoming connections first
	for (auto& connection : node->InputConnections)
		InsertNode(list, connection.Incoming);

	// otherwise we insert it into the sorted list
	list.emplace_back(node);
	node->States.TraversalState = GraphTraversalState::eVisited;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::Update() const
{
	for (const auto& [name, node] : Nodes)
		node->UpdateFn(*node);
}

AQUA_NAMESPACE::EXEC_NAMESPACE::GraphList AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::SortEntries() const
{
	std::lock_guard guard(*Lock);

	GraphList list;
	list.reserve(Nodes.size());

	for (auto& node : Nodes)
		node.second->States.TraversalState = GraphTraversalState::ePending;

	for (const auto& path : Paths)
	{
		InsertNode(list, Nodes.at(path));
	}

	return list;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::Validate() const
{
	for (const auto& probe : Paths)
	{
		if (FindClosedCircuit(Nodes.at(probe)))
			return std::unexpected(GraphError::eFoundEmbeddedCircuit);

		for (auto& node : Nodes)
			node.second->States.TraversalState = GraphTraversalState::ePending;
	}

	return true;
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::ClearInputInjections()
{
	for (auto& [name, op] : Nodes)
	{
		op->InputInjections.clear();
	}
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::ClearOutputInjections()
{
	for (auto& [name, op] : Nodes)
	{
		op->OutputInjections.clear();
	}
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
	AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::InjectInputDependencies(
		const vk::ArrayProxy<DependencyInjection>& injections)
{
	for (const auto& injection : injections)
	{
		if (Nodes.find(injection.ConnectedOp) == Nodes.end())
			return std::unexpected(GraphError::eInjectedOpDoesntExist);
	}

	for (const auto& injection : injections)
	{
		auto op = Nodes.at(injection.ConnectedOp);
		op->AddInputInjection(injection);
	}

	return true;
}

std::expected<bool, AQUA_NAMESPACE::EXEC_NAMESPACE::GraphError> 
AQUA_NAMESPACE::EXEC_NAMESPACE::Graph::InjectOutputDependencies(
	const vk::ArrayProxy<DependencyInjection>& injections)
{
	for (const auto& injection : injections)
	{
		if (Nodes.find(injection.ConnectedOp) == Nodes.end())
			return std::unexpected(GraphError::eInjectedOpDoesntExist);
	}

	for (const auto& injection : injections)
	{
		auto op = Nodes.at(injection.ConnectedOp);
		op->AddOutputInjection(injection);
	}

	return true;
}
