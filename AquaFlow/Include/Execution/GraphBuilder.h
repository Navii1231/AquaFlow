#pragma once
#include "GraphConfig.h"
#include "Executioner.h"
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

class GraphBuilder
{
public:
	GraphBuilder() = default;
	~GraphBuilder() = default;

	void SetCtx(vkLib::Context ctx) { mCtx = ctx; }

	void Clear();
	void ClearOperations() { mOperations.clear(); }
	void ClearDependencies() { mInputDependencies.clear(); }

	// internal dependencies
	void InsertDependency(const std::string& from, const std::string& to,
		vk::PipelineStageFlags stageFlags = vk::PipelineStageFlagBits::eTopOfPipe);

	void InsertPipelineOp(const std::string& name, vkLib::BasicPipeline& pipeline);

	// Redundant operations that do no contribute to the final outcome are excluded
	// absolutely convergent on the probe node
	std::expected<Graph, GraphError> GenerateExecutionGraph(const vk::ArrayProxy<std::string>& pathEnds) const;
	// Build the execution graph and sorts the entries out according to their execution order
	Operation& operator[](const std::string& name)
	{ return mOperations[name]; }

	const Operation& operator[](const std::string& name) const
	{ return mOperations.at(name); }

private:
	std::unordered_map<std::string, Operation> mOperations;

	// From "To" nodes --> Dependencies
	mutable std::unordered_map<std::string, std::vector<DependencyMetaData>> mInputDependencies;

	vkLib::Context mCtx;

private:
	std::expected<bool, GraphError> BuildDependencySkeleton(std::unordered_map<std::string, std::shared_ptr<Operation>>& opCache, 
		const std::string& name, const vk::ArrayProxy<std::string>& probes) const;
	// After the graph is validated, we create and emplace the semaphores between dependent operations
	void EmplaceSemaphore(Dependency& connection) const;

	std::expected<bool, GraphError> ValidateDependencyInputs() const;
	std::shared_ptr<Operation> CreateOperation(const std::string& name) const;
};

EXEC_END
AQUA_END
