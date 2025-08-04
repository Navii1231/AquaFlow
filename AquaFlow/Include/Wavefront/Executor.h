#pragma once
#include "ExecutorConfig.h"

#include "../Execution/GraphBuilder.h"

AQUA_BEGIN
PH_BEGIN

// Incorporating the execution model next
class Executor
{
public:
	Executor();

	// Copy the command buffers
	Executor(const Executor& Other);
	Executor& operator=(const Executor& Other);

	~Executor();

	TraceResult Trace();

	void Validate() { ConstructExecutionGraphs(mDepth); }

	void SetDepth(uint32_t depth) { mDepth = depth; }
	void SetTraceSession(const TraceSession& traceSession);

	template<typename Iter>
	void SetMaterialPipelines(Iter Begin, Iter End);

	void SetSortingFlag(bool allowSort)
	{ mExecutorInfo->CreateInfo.AllowSorting = allowSort; }

	void SetCameraView(const glm::mat4& cameraView);

	// Getters...
	TraceSession GetTraceSession() const { return mExecutorInfo->TracingSession; }

	glm::ivec2 GetTargetResolution() const { return mExecutorInfo->Target.ImageResolution; }
	vkLib::Image GetPresentable() const { return mExecutorInfo->Target.Presentable; }
	vkLib::Buffer<WavefrontSceneInfo> GetSceneInfo() const { return mExecutorInfo->Scene; }

	// For debugging...
	RayBuffer GetRayBuffer() const { return mExecutorInfo->Rays; }
	CollisionInfoBuffer GetCollisionBuffer() const { return mExecutorInfo->CollisionInfos; }
	RayRefBuffer GetRayRefBuffer() const { return mExecutorInfo->RayRefs; }
	RayInfoBuffer GetRayInfoBuffer() const { return mExecutorInfo->RayInfos; }
	vkLib::Buffer<uint32_t> GetMaterialRefCounts() const { return mExecutorInfo->RefCounts; }
	vkLib::Image GetVariance() const { return mExecutorInfo->Target.PixelVariance; }
	vkLib::Image GetMean() const { return mExecutorInfo->Target.PixelMean; }

private:
	std::shared_ptr<ExecutionInfo> mExecutorInfo;

	uint32_t mDepth = 0;
	PostProcessFlags mPostProcess;

	EXEC_NAMESPACE::Graph mTraceGraph;
	EXEC_NAMESPACE::GraphBuilder mGraphBuilder;

	EXEC_NAMESPACE::GraphList mTraceExecList;

	ExecutionBlock mExecutionBlock;

	std::vector<vk::CommandBuffer> mCmdBufs;

private:
	Executor(const ExecutorCreateInfo& createInfo);

	void ConstructExecutionGraphs(uint32_t depth);

	uint32_t GetRandomNumber();

	void RecordRayGenerator(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);

	void ExecuteRaySortFinisher(vk::CommandBuffer commandBuffer);
	void ExecutePrefixSummer(vk::CommandBuffer commandBuffer);
	void ExecuteRayCounter(vk::CommandBuffer commandBuffer);
	void ExecuteRaySortPreparer(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);
	void RecordIntersectionTester(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);
	void RecordLuminanceMean(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);
	void RecordPostProcess(vk::CommandBuffer commandBuffer);

	void UpdateSceneInfo();

	void InvalidateMaterialData();

private:
	void AssignMaterialsResources(MaterialInstance& instance, const SessionInfo& TracingSession);
	void RecordMaterialPipeline(vk::CommandBuffer cmd, uint32_t pMaterialRef, uint32_t pBounceIdx, uint32_t pActiveBuffer);

	void UpdateMaterialDescriptors();

	void Sweep(EXEC_NAMESPACE::GraphBuilder& builder, uint32_t currDepth, const std::string& closingOp = "");

	friend class WavefrontEstimator;
};

template<typename Iter>
inline void Executor::SetMaterialPipelines(Iter Begin, Iter End)
{
	mExecutorInfo->MaterialResources.clear();
	
	for (; Begin != End; Begin++)
	{
		mExecutorInfo->MaterialResources.emplace_back(*Begin);

		// For optimizations...
		if (mExecutorInfo->MaterialResources.size() == mExecutorInfo->MaterialResources.capacity())
			mExecutorInfo->MaterialResources.reserve(2 * mExecutorInfo->MaterialResources.size());
	}

	mExecutorInfo->RefCounts.Resize(glm::max(static_cast<uint32_t>(mExecutorInfo->MaterialResources.size()
		+ 2), static_cast<uint32_t>(32)));

	InvalidateMaterialData();
}

PH_END
AQUA_END
