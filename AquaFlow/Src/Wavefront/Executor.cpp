#include "Core/Aqpch.h"
#include "Wavefront/Executor.h"

#define MAX_UINT32 (static_cast<uint32_t>(~0))

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Executor() {}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Executor(const Executor& Other)
	: mExecutorInfo(Other.mExecutorInfo)
{
	mGraphBuilder = Other.mGraphBuilder;
	mGraphBuilder.Clear();

	mCmdBufs.reserve(Other.mCmdBufs.size());

	for (size_t i = 0; i < mCmdBufs.capacity(); i++)
	{
		mCmdBufs.push_back(mExecutorInfo->CmdAlloc.Allocate());
	}
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor& AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::operator=(const Executor& Other)
{
	mGraphBuilder = Other.mGraphBuilder;
	mGraphBuilder.Clear();

	mExecutorInfo = Other.mExecutorInfo;

	mCmdBufs.reserve(Other.mCmdBufs.size());

	for (size_t i = 0; i < mCmdBufs.capacity(); i++)
	{
		mCmdBufs.push_back(mExecutorInfo->CmdAlloc.Allocate());
	}

	return *this;
}

AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::~Executor()
{
	for (auto cmdBuf : mCmdBufs)
		mExecutorInfo->CmdAlloc.Free(cmdBuf);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ConstructExecutionGraphs(uint32_t depth)
{
	if (depth == 0)
		return;

	std::string rayGenerationName = "@(ray_generation)";
	std::string firstIntersectName = "@(intersection_test)._0";
	std::string postProcessName = "@(post_process)";
	std::string luminanceName = "@(calc_luminance)";

	mGraphBuilder.Clear();

	mGraphBuilder.InsertPipelineOp(rayGenerationName, mExecutorInfo->PipelineResources.RayGenerator);
	mGraphBuilder.InsertDependency(rayGenerationName, firstIntersectName);

	mGraphBuilder[rayGenerationName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::Executioner executioner(cmd, op);
			RecordRayGenerator(cmd, mExecutionBlock.ActiveBuffer);
		});

#if 1
	for (uint32_t i = 0; i < depth - 1; i++)
		Sweep(mGraphBuilder, i);

	Sweep(mGraphBuilder, depth - 1, luminanceName);

#endif

	mGraphBuilder.InsertPipelineOp(luminanceName, mExecutorInfo->PipelineResources.LuminanceMean);
	mGraphBuilder.InsertPipelineOp(postProcessName, mExecutorInfo->PipelineResources.PostProcessor);

	mGraphBuilder[luminanceName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::Executioner executioner(cmd, op);
			RecordLuminanceMean(cmd, mExecutionBlock.ActiveBuffer);
		});

	mGraphBuilder[postProcessName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::Executioner executioner(cmd, op);
			RecordPostProcess(cmd);
		});

	mGraphBuilder.InsertDependency(luminanceName, postProcessName);

	mTraceGraph = *mGraphBuilder.GenerateExecutionGraph(luminanceName);
	mTraceExecList = mTraceGraph.SortEntries();
}

uint32_t AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::GetRandomNumber()
{
	uint32_t Random = mExecutorInfo->UniformDistribution(mExecutorInfo->RandomEngine);
	_STL_ASSERT(Random != 0, "Random number can't be zero!");

	return Random;
}


AQUA_NAMESPACE::PH_FLUX_NAMESPACE::TraceResult AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Trace()
{
	UpdateSceneInfo();

	mExecutionBlock = {};

	auto& execList = mTraceExecList;

	auto executor = mExecutorInfo->Workers;

	for (size_t i = 0; i < execList.size(); i++)
	{
		auto& op = *execList[i];

		size_t queueIdx = i % executor.GetQueueCount();

		executor[queueIdx]->WaitIdle();
		op(mCmdBufs[queueIdx], executor);
	}

	return TraceResult::eComplete;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetTraceSession(const TraceSession& traceSession)
{
	_STL_ASSERT(traceSession.GetState() != TraceSessionState::eOpenScope,
		"Can't execute the trace session in the eOpenScope state!");

	mExecutorInfo->TracingSession = traceSession;
	mExecutorInfo->TracingInfo = traceSession.mSessionInfo->TraceInfo;

	auto& pipelines = mExecutorInfo->PipelineResources;

	pipelines.RayGenerator.mCamera = traceSession.mSessionInfo->CameraSpecsBuffer;
	pipelines.RayGenerator.mRays = mExecutorInfo->Rays;
	pipelines.RayGenerator.mSceneInfo = mExecutorInfo->Scene;
	pipelines.RayGenerator.mRayInfos = mExecutorInfo->RayInfos;

	pipelines.IntersectionPipeline.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.IntersectionPipeline.mRays = mExecutorInfo->Rays;
	pipelines.IntersectionPipeline.mSceneInfo = mExecutorInfo->Scene;
	pipelines.IntersectionPipeline.mGeometryBuffers = traceSession.mSessionInfo->LocalBuffers;
	pipelines.IntersectionPipeline.mLightInfos = traceSession.mSessionInfo->LightInfos;
	pipelines.IntersectionPipeline.mLightProps = traceSession.mSessionInfo->LightPropsInfos;
	pipelines.IntersectionPipeline.mMeshInfos = traceSession.mSessionInfo->MeshInfos;

	pipelines.PrefixSummer.mRefCounts = mExecutorInfo->RefCounts;

	pipelines.RayRefCounter.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RayRefCounter.mRefCounts = mExecutorInfo->RefCounts;

	pipelines.RaySortPreparer.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.RaySortPreparer.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RaySortPreparer.mRays = mExecutorInfo->Rays;
	pipelines.RaySortPreparer.mRaysInfos = mExecutorInfo->RayInfos;

	pipelines.RaySortFinisher.mRays = mExecutorInfo->Rays;
	pipelines.RaySortFinisher.mRayRefs = mExecutorInfo->RayRefs;
	pipelines.RaySortFinisher.mCollisionInfos = mExecutorInfo->CollisionInfos;
	pipelines.RaySortFinisher.mRaysInfos = mExecutorInfo->RayInfos;

	AssignMaterialsResources(pipelines.InactiveRayShader, *traceSession.mSessionInfo);

	pipelines.LuminanceMean.mPixelMean = mExecutorInfo->Target.PixelMean;
	pipelines.LuminanceMean.mPixelVariance = mExecutorInfo->Target.PixelVariance;
	pipelines.LuminanceMean.mPresentable = mExecutorInfo->Target.Presentable;
	pipelines.LuminanceMean.mRays = mExecutorInfo->Rays;
	pipelines.LuminanceMean.mRayInfos = mExecutorInfo->RayInfos;
	pipelines.LuminanceMean.mSceneInfo = mExecutorInfo->Scene;

	pipelines.PostProcessor.mPresentable = mExecutorInfo->Target.Presentable;

	InvalidateMaterialData();

	pipelines.RayGenerator.UpdateDescriptors();
	pipelines.IntersectionPipeline.UpdateDescriptors();
	pipelines.PrefixSummer.UpdateDescriptors();
	pipelines.RayRefCounter.UpdateDescriptors();
	pipelines.RaySortPreparer.UpdateDescriptors();
	pipelines.RaySortFinisher.UpdateDescriptors();
	pipelines.LuminanceMean.UpdateDescriptors();
	pipelines.PostProcessor.UpdateDescriptors();
	pipelines.InactiveRayShader.UpdateDescriptors();

	UpdateMaterialDescriptors();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::SetCameraView(const glm::mat4& cameraView)
{
	if (!mExecutorInfo->TracingSession.mSessionInfo)
		return;

	mExecutorInfo->TracingInfo.CameraView = cameraView;
	mExecutorInfo->TracingSession.SetCameraView(cameraView);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordRayGenerator(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	auto workGroupSize = mExecutorInfo->PipelineResources.RayGenerator.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RayGenerator.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RayGenerator.Activate();

	mExecutorInfo->PipelineResources.RayGenerator.SetShaderConstant("eCompute.Camera.Index_0", mExecutorInfo->TracingInfo.CameraView);
	mExecutorInfo->PipelineResources.RayGenerator.SetShaderConstant("eCompute.Camera.Index_1", GetRandomNumber());
	mExecutorInfo->PipelineResources.RayGenerator.SetShaderConstant("eCompute.Camera.Index_2", pActiveBuffer);

	mExecutorInfo->PipelineResources.RayGenerator.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RayGenerator.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ExecuteRaySortFinisher(vk::CommandBuffer commandBuffer)
{
	auto workGroupSize = mExecutorInfo->PipelineResources.RaySortFinisher.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RaySortFinisher.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RaySortFinisher.Activate();

	mExecutorInfo->PipelineResources.RaySortFinisher.SetShaderConstant("eCompute.RayData.Index_0", pRayCount);
	mExecutorInfo->PipelineResources.RaySortFinisher.SetShaderConstant("eCompute.RayData.Index_1", mExecutionBlock.ActiveBuffer);
	mExecutorInfo->PipelineResources.RaySortFinisher.SetShaderConstant("eCompute.RayData.Index_2", 0 /*comes from the sorting rays*/);

	mExecutorInfo->PipelineResources.RaySortFinisher.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RaySortFinisher.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ExecutePrefixSummer(vk::CommandBuffer commandBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	mExecutorInfo->PipelineResources.PrefixSummer.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.PrefixSummer.Activate();

	mExecutorInfo->PipelineResources.PrefixSummer.SetShaderConstant("eCompute.MetaData.Index_0", pMaterialCount);

	mExecutorInfo->PipelineResources.PrefixSummer.Dispatch({ 1, 1, 1 });

	mExecutorInfo->PipelineResources.PrefixSummer.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ExecuteRayCounter(vk::CommandBuffer commandBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.RayRefCounter.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RayRefCounter.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RayRefCounter.Activate();

	mExecutorInfo->PipelineResources.RayRefCounter.SetShaderConstant("eCompute.MetaData.Index_0", pRayCount);
	mExecutorInfo->PipelineResources.RayRefCounter.SetShaderConstant("eCompute.MetaData.Index_1", pMaterialCount);

	mExecutorInfo->PipelineResources.RayRefCounter.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RayRefCounter.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::ExecuteRaySortPreparer(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.RaySortPreparer.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.RaySortPreparer.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.RaySortPreparer.Activate();

	mExecutorInfo->PipelineResources.RaySortPreparer.SetShaderConstant("eCompute.RayData.Index_0", pRayCount);
	mExecutorInfo->PipelineResources.RaySortPreparer.SetShaderConstant("eCompute.RayData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.RaySortPreparer.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.RaySortPreparer.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordIntersectionTester(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.IntersectionPipeline.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.IntersectionPipeline.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.IntersectionPipeline.Activate();

	mExecutorInfo->PipelineResources.IntersectionPipeline.SetShaderConstant("eCompute.RayData.Index_0", pRayCount);
	mExecutorInfo->PipelineResources.IntersectionPipeline.SetShaderConstant("eCompute.RayData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.IntersectionPipeline.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.IntersectionPipeline.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordLuminanceMean(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer)
{
	uint32_t pMaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size() + 2);

	auto workGroupSize = mExecutorInfo->PipelineResources.LuminanceMean.GetWorkGroupSize().x;
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / workGroupSize + 1, 1, 1 };

	mExecutorInfo->PipelineResources.LuminanceMean.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.LuminanceMean.Activate();

	mExecutorInfo->PipelineResources.LuminanceMean.SetShaderConstant("eCompute.ShaderData.Index_0", pRayCount);
	mExecutorInfo->PipelineResources.LuminanceMean.SetShaderConstant("eCompute.ShaderData.Index_1", pActiveBuffer);

	mExecutorInfo->PipelineResources.LuminanceMean.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.LuminanceMean.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordPostProcess(vk::CommandBuffer commandBuffer)
{
	glm::uvec3 rayGroupSize = mExecutorInfo->PipelineResources.RayGenerator.GetWorkGroupSize();
	glm::uvec3 workGroups = { mExecutorInfo->CreateInfo.TileSize.x / rayGroupSize.x,
	mExecutorInfo->CreateInfo.TileSize.y / rayGroupSize.y, 1 };

	mExecutorInfo->PipelineResources.PostProcessor.Begin(commandBuffer);

	mExecutorInfo->PipelineResources.PostProcessor.Activate();

	mExecutorInfo->PipelineResources.PostProcessor.SetShaderConstant("eCompute.ShaderData.Index_0", mExecutorInfo->CreateInfo.TileSize.x);
	mExecutorInfo->PipelineResources.PostProcessor.SetShaderConstant("eCompute.ShaderData.Index_1", mExecutorInfo->CreateInfo.TileSize.y);
	mExecutorInfo->PipelineResources.PostProcessor.SetShaderConstant("eCompute.ShaderData.Index_2", (uint32_t) (int) mPostProcess);

	mExecutorInfo->PipelineResources.PostProcessor.Dispatch(workGroups);

	mExecutorInfo->PipelineResources.PostProcessor.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::UpdateSceneInfo()
{
	WavefrontSceneInfo& sceneInfo = mExecutorInfo->TracingSession.mSessionInfo->SceneData;
	sceneInfo.ImageResolution = mExecutorInfo->CreateInfo.TargetResolution;
	sceneInfo.MinBound = { 0, 0 };
	sceneInfo.MaxBound = mExecutorInfo->CreateInfo.TargetResolution;
	sceneInfo.FrameCount = mExecutorInfo->TracingSession.mSessionInfo->State == TraceSessionState::eReady ?
		1 : sceneInfo.FrameCount + 1;

	mExecutorInfo->Scene.Clear();
	mExecutorInfo->Scene << sceneInfo;

	mExecutorInfo->TracingSession.mSessionInfo->State = TraceSessionState::eTracing;

	ShaderData shaderData{};
	shaderData.uRayCount = (uint32_t) mExecutorInfo->Rays.GetSize();
	shaderData.uSkyboxColor = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
	shaderData.uSkyboxExists = false;

	mExecutorInfo->TracingSession.mSessionInfo->ShaderConstData.Clear();
	mExecutorInfo->TracingSession.mSessionInfo->ShaderConstData << shaderData;
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::InvalidateMaterialData()
{
	if (!mExecutorInfo->TracingSession)
		return;

	auto& TracingSession = *mExecutorInfo->TracingSession.mSessionInfo;

	for (auto& curr : mExecutorInfo->MaterialResources)
		AssignMaterialsResources(curr, TracingSession);

	auto& inactivePipeline = mExecutorInfo->PipelineResources.InactiveRayShader;
	AssignMaterialsResources(inactivePipeline, TracingSession);
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::AssignMaterialsResources(MaterialInstance& instance, const SessionInfo& TracingSession)
{
	instance[{ 0, 0, 0 }].SetStorageBuffer(mExecutorInfo->Rays.GetBufferChunk());
	instance[{ 0, 1, 0 }].SetStorageBuffer(mExecutorInfo->RayInfos.GetBufferChunk());
	instance[{ 0, 2, 0 }].SetStorageBuffer(mExecutorInfo->CollisionInfos.GetBufferChunk());
	instance[{ 0, 3, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Vertices.GetBufferChunk());
	instance[{ 0, 4, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Normals.GetBufferChunk());
	instance[{ 0, 5, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.TexCoords.GetBufferChunk());
	instance[{ 0, 6, 0 }].SetStorageBuffer(TracingSession.LocalBuffers.Faces.GetBufferChunk());
	instance[{ 0, 7, 0 }].SetStorageBuffer(TracingSession.LightInfos.GetBufferChunk());
	instance[{ 0, 8, 0 }].SetStorageBuffer(TracingSession.LightPropsInfos.GetBufferChunk());
	instance[{ 1, 0, 0 }].SetUniformBuffer(TracingSession.ShaderConstData.GetBufferChunk());
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::RecordMaterialPipeline(vk::CommandBuffer commandBuffer, uint32_t pMaterialRef, uint32_t pBounceIdx, uint32_t pActiveBuffer)
{
	uint32_t pRayCount = static_cast<uint32_t>(mExecutorInfo->Rays.GetSize()) / 2;
	glm::uvec3 workGroups = { pRayCount / 256, 1, 1 };

	uint32_t MaterialCount = static_cast<uint32_t>(mExecutorInfo->MaterialResources.size());

	const vkLib::ComputePipeline* pipelinePtr = nullptr; 
	
	if (pMaterialRef != -1)
		pipelinePtr = reinterpret_cast<const vkLib::ComputePipeline*>(mExecutorInfo->MaterialResources[pMaterialRef].GetBasicPipeline());
	else
		pipelinePtr = reinterpret_cast<const vkLib::ComputePipeline*>(mExecutorInfo->PipelineResources.InactiveRayShader.GetBasicPipeline());

	const vkLib::ComputePipeline& pipeline = *pipelinePtr;

	pipeline.Begin(commandBuffer);

	pipeline.Activate();

	pipeline.SetShaderConstant("eCompute.ShaderConstants.Index_0", pMaterialRef);
	pipeline.SetShaderConstant("eCompute.ShaderConstants.Index_1", pActiveBuffer);
	
	if(pMaterialRef != -1)
		pipeline.SetShaderConstant("eCompute.ShaderConstants.Index_2", GetRandomNumber());
	//pipeline.SetShaderConstant("eCompute.ShaderConstants.Index_3", pBounceIdx);

	pipeline.Dispatch(workGroups);

	pipeline.End();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::UpdateMaterialDescriptors()
{
	for (auto& instance : mExecutorInfo->MaterialResources)
	{
		instance.UpdateDescriptors();
	}

	mExecutorInfo->PipelineResources.InactiveRayShader.UpdateDescriptors();
}

void AQUA_NAMESPACE::PH_FLUX_NAMESPACE::Executor::Sweep(EXEC_NAMESPACE::GraphBuilder& builder, uint32_t currDepth, const std::string& closingOp)
{
	std::string intersectionName = "@(intersection_test)._" + std::to_string(currDepth);
	std::string nextIntersectName = "@(intersection_test)._" + std::to_string(currDepth + 1);
	std::string materialName = "@(material)._" + std::to_string(currDepth) + "_";
	std::string emptyMaterial = "@(empty_material)._" + std::to_string(currDepth);

	builder.InsertPipelineOp(intersectionName, mExecutorInfo->PipelineResources.IntersectionPipeline);

	builder[intersectionName].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::Executioner executioner(cmd, op);
			RecordIntersectionTester(cmd, mExecutionBlock.BounceIdx++);
		});

	uint32_t instanceIdx = 0;
	
	for (const auto& materialInstance : mExecutorInfo->MaterialResources)
	{
		std::string instanceName = materialName + std::to_string(instanceIdx);

		builder[instanceName] = materialInstance.GetMaterial();

		builder[instanceName].SetOpFn([this, instanceIdx](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
			{
				EXEC_NAMESPACE::Executioner executioner(cmd, op);
				RecordMaterialPipeline(cmd, instanceIdx, mExecutionBlock.BounceIdx - 1, mExecutionBlock.ActiveBuffer);
			});

		instanceIdx++;

		builder.InsertDependency(intersectionName, instanceName);
		builder.InsertDependency(instanceName, closingOp.empty() ? nextIntersectName : closingOp);
	}

	builder[emptyMaterial] = mExecutorInfo->PipelineResources.InactiveRayShader.GetMaterial();

	builder[emptyMaterial].SetOpFn([this](vk::CommandBuffer cmd, const EXEC_NAMESPACE::Operation& op)
		{
			EXEC_NAMESPACE::Executioner executioner(cmd, op);
			RecordMaterialPipeline(cmd, -1, mExecutionBlock.BounceIdx - 1, mExecutionBlock.ActiveBuffer);
		});

	builder.InsertDependency(intersectionName, emptyMaterial);
	builder.InsertDependency(emptyMaterial, closingOp.empty() ? nextIntersectName : closingOp);
}

