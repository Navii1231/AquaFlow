#pragma once
#include "GraphConfig.h"

AQUA_BEGIN
EXEC_BEGIN

struct OpInput
{
	vk::CommandBuffer Cmds{};
	std::binary_semaphore* Signal{};
};

struct DependencyInjection
{
	std::string ConnectedOp;

	vk::PipelineStageFlags WaitPoint = vk::PipelineStageFlagBits::eTopOfPipe;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void Connect(const std::string& connect) { ConnectedOp = connect; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }
};

struct Dependency
{
	// The connection on which the dependency is formed
	std::shared_ptr<Operation> Incoming;
	std::shared_ptr<Operation> Outgoing;

	vk::PipelineStageFlags WaitPoint;
	vkLib::Core::Ref<vk::Semaphore> Signal;

	void SetIncomingOP(std::shared_ptr<Operation> connection) { Incoming = connection; }
	void SetOutgoingOP(std::shared_ptr<Operation> connection) { Outgoing = connection; }
	void SetWaitPoint(vk::PipelineStageFlags waitPoint) { WaitPoint = waitPoint; }
	void SetSignal(vkLib::Core::Ref<vk::Semaphore> semaphore) { Signal = semaphore; }
};

using OpFn = std::function<void(vk::CommandBuffer, const Operation&)>;
using OpUpdateFn = std::function<void(Operation&)>;

using SemaphoreList = std::vector<vk::Semaphore>;
using PipelineStageList = std::vector<vk::PipelineStageFlags>;

struct Operation
{
	std::string Name;

	// execution data; completely removable if we have Fn as a lambda
	std::shared_ptr<vkLib::GraphicsPipeline> GFX;
	std::shared_ptr<vkLib::ComputePipeline> Cmp;
	// vkLib::RayTracingPipeline RTX;

	mutable OpStates States;

	OpFn Fn = [](vk::CommandBuffer, const Operation&) {};
	OpUpdateFn UpdateFn = [](Operation&) {}; // could be used to update descriptors

	std::vector<Dependency> InputConnections;  // Operations that must finish before triggering this one
	std::vector<Dependency> OutputConnections; // Operations that can't begin before this one is finished

	std::vector<DependencyInjection> InputInjections; // an external event must finish before this one starts
	std::vector<DependencyInjection> OutputInjections; // an external event is dependent upon this one

	uint64_t OpID = 0; // operations id; don't know why I need it, but it kinda makes the renderer internally consistent

	void operator()(vk::CommandBuffer cmd, vkLib::Core::Executor executor, std::binary_semaphore* signal = nullptr) const;
	void operator()(vk::CommandBuffer cmd, vkLib::Core::Ref<vkLib::Core::Queue> worker, std::binary_semaphore* signal = nullptr) const;

	void SetOpFn(OpFn&& fn) { Fn = fn; }

	void AddInputConnection(const Dependency& dependency) { InputConnections.emplace_back(dependency); }
	void AddOutputConnection(const Dependency& dependency) { OutputConnections.emplace_back(dependency); }

	void AddInputInjection(const DependencyInjection& inj) { InputInjections.emplace_back(inj); }
	void AddOutputInjection(const DependencyInjection& inj) { OutputInjections.emplace_back(inj); }

	OpType GetOpType() const { return States.Type; }

	std::expected<const vkLib::BasicPipeline*, OpType> GetBasicPipeline() const;
	std::expected<vkLib::BasicPipeline*, OpType> GetBasicPipeline();
	bool Execute(vk::CommandBuffer cmd, vkLib::Core::Executor workers, std::binary_semaphore* signal = nullptr) const;
	bool Execute(vk::CommandBuffer cmd, vkLib::Core::Ref<vkLib::Core::Queue> worker, std::binary_semaphore* signal = nullptr) const;

	vk::SubmitInfo SetupSubmitInfo(vk::CommandBuffer& cmd, SemaphoreList& waitingPoints,
		SemaphoreList& signalList, PipelineStageList& pipelineStages) const;

	Operation() = default;
};

using GraphList = std::vector<std::shared_ptr<Operation>>;
using GraphOps = std::unordered_map<std::string, std::shared_ptr<Operation>>;

// Recursive function to generate the sorted array of operations
void InsertNode(GraphList& list, std::shared_ptr<Operation> node);
bool FindClosedCircuit(std::shared_ptr<Operation> node);

struct Graph
{
	std::vector<std::string> Paths;
	GraphOps Nodes;

	std::shared_ptr<std::mutex> Lock;

	void Update() const;
	GraphList SortEntries() const;
	std::expected<bool, GraphError> Validate() const;

	void ClearInputInjections();
	void ClearOutputInjections();

	// external dependencies
	std::expected<bool, GraphError> InjectInputDependencies(const vk::ArrayProxy<DependencyInjection>& injections);
	std::expected<bool, GraphError> InjectOutputDependencies(const vk::ArrayProxy<DependencyInjection>& injections);

};

EXEC_END
AQUA_END
