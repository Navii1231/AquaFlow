#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderer/Renderer.h"
#include "DeferredRenderer/RenderGraph/CopyPlugin.h"
#include "DeferredRenderer/RenderGraph/GBufferPlugin.h"
#include "DeferredRenderer/RenderGraph/ShadowPlugin.h"
#include "DeferredRenderer/RenderGraph/SkyboxPlugin.h"
#include "DeferredRenderer/RenderGraph/PostProcessPlugin.h"

#include "DeferredRenderer/Renderable/CopyIndices.h"

#include "DeferredRenderer/Renderable/RenderTargetFactory.h"

#include "DeferredRenderer/Renderer/FrontEndGraph.h"
#include "DeferredRenderer/Renderer/BackEndGraph.h"
#include "DeferredRenderer/Renderer/MaterialSystem.h"
#include "DeferredRenderer/Renderer/BezierCurve.h"

#include "DeferredRenderer/Renderable/RenderableBuilder.h"

#include "Utils/CompilerErrorChecker.h"
#include <semaphore>

AQUA_BEGIN

extern RenderableBuilder::CopyVertFnMap sVertexCopyFn;
extern RenderableBuilder::CopyIdxFn sIndexCopyFn;

struct RendererConfig
{
	RendererConfig() = default;
	~RendererConfig() = default;

	struct MaterialInfo
	{
		MAT_NAMESPACE::Material Op;
		SharedRef<MaterialInstanceInfo> Info;
	};

	// Renderable stuff
	std::vector<MaterialInfo> mMaterials;

	std::unordered_map<std::string, vkLib::GenericBuffer> mHSLinesVertices;
	std::unordered_map<std::string, vkLib::GenericBuffer> mHSPointsVertices;
	std::unordered_map<std::string, Renderable> mRenderables;
	// SUGGESTION: we could use one giant buffer and map the ranges within the buffer for each renderable
	std::unordered_map<std::string, vkLib::GenericBuffer> mVertexMetaBuffers;

	// the things that will be rendered
	std::unordered_set<std::string> mActiveRenderables;
	std::unordered_set<std::string> mActiveLines;
	std::unordered_set<std::string> mActivePoints;

	uint32_t mRenderableCount = 0;
	uint32_t mReservedVertices = 1000;
	uint32_t mReservedIndices = 1000;
	VertexBindingMap mVertexBindingsInfo;
	VertexFactory mVertexFactory;

	// hyper surface vertices
	VertexFactory mHSVLinesFactory;
	VertexFactory mHSVPointsFactory;

	RenderableBuilder mRenderableBuilder = RenderableBuilder(sVertexCopyFn, sIndexCopyFn);

	// environments
	EnvironmentRef mEnv;
	Mat4Buf mModels;

	std::unordered_map<RenderingFeature, FeatureInfo> mFeatureInfos;

	vkLib::Buffer<FeaturesEnabled> mFeatures;
	vkLib::Buffer<CameraInfo> mCamera;

	RendererFeatureFlags mFeatureFlags;

	// Feature implementations
	FrontEndGraph mFrontEnd;
	BackEndGraph mBackEnd;

	// Framebuffers...
	vkLib::Framebuffer mGBuffer;
	vkLib::Framebuffer mShadingbuffer;

	RenderTargetFactory mRenderCtxFactory;
	RenderTargetFactory mHSTargetCtx;

	vkLib::Core::Ref<vk::Sampler> mShadingSampler;
	vkLib::Core::Ref<vk::Sampler> mDepthSampler;

	MaterialSystem mMaterialSystem;

	// CPU synchronization
	std::vector<SharedRef<std::binary_semaphore>> mCPUSignals;

	// Execution stuff...
	EXEC_NAMESPACE::Graph mShadingNetwork; // characterized by geometry buffer and material arrays
	EXEC_NAMESPACE::GraphBuilder mRenderGraphBuilder;
	EXEC_NAMESPACE::GraphList mShadingNetworkGraphList;

	std::vector<CopyIdxPipeline> mCopyIndices;

	std::vector<vk::CommandBuffer> mCmdBufs;
	std::vector<vk::CommandBuffer> mRenderableCmds;
	vkLib::Core::Executor mWorkers;
	vkLib::CommandBufferAllocator mCmdAlloc;

	vkLib::PipelineBuilder mPipelineBuilder;
	vkLib::ResourcePool mResourcePool;
	vkLib::Context mCtx;

	// Shader stuff...
	std::string mShaderDirectory = "D:\\Dev\\AquaFlow\\AquaFlow\\Assets\\Shaders\\Deferred\\";
	vkLib::PShader mGBufferShader;
	vkLib::PShader mSkyboxShader;
	vkLib::PShader mCopyIdxShader;

	constexpr static uint64_t sMatTypeID = -1;
	constexpr static uint64_t sGBufferID = -2;
};

void SetFeatures(RendererFeatureFlags flags, vkLib::Buffer<FeaturesEnabled> enabled, bool val)
{
	std::vector<FeaturesEnabled> features;

	enabled >> features;

	auto AssignFeature = [flags, val](RenderingFeature featFlag, uint32_t& featVal)
		{
			if (flags & RendererFeatureFlags(featFlag))
				featVal = val;
		};

	AssignFeature(RenderingFeature::eSSAO, features[0].SSAO);
	AssignFeature(RenderingFeature::eShadow, features[0].ShadowMapping);
	AssignFeature(RenderingFeature::eBloomEffect, features[0].BloomEffect);

	enabled.SetBuf(features.begin(), features.end());
}

std::vector<RendererConfig::MaterialInfo>::const_iterator FindMaterialInstance(
	MaterialInstance& lineMaterial, SharedRef<RendererConfig> config)
{
	return std::find_if(config->mMaterials.begin(), config->mMaterials.end(),
		[&lineMaterial, config](const RendererConfig::MaterialInfo& materialInfo)
		{
			if (materialInfo.Op.GFX->GetPipelineHandles() == lineMaterial.GetMaterial().GFX->GetPipelineHandles()
				&& materialInfo.Info == lineMaterial.GetInfo())
				return true;

			return false;
		});
}

AQUA_END

AQUA_NAMESPACE::Renderer::Renderer()
{
	mConfig = std::make_shared<RendererConfig>();
	SetupVertexBindings();
	SetupShaders();
}

AQUA_NAMESPACE::Renderer::~Renderer()
{
	if (!mConfig)
		return;

	for (auto buf : mConfig->mCmdBufs)
	{
		mConfig->mCmdAlloc.Free(buf);
	}
}

AQUA_NAMESPACE::RendererFeatureFlags AQUA_NAMESPACE::Renderer::GetEnabledFeatures()
{
	return mConfig->mFeatureFlags;
}

void AQUA_NAMESPACE::Renderer::SetCtx(vkLib::Context ctx)
{
	mConfig->mCtx = ctx;
	mConfig->mWorkers = mConfig->mCtx.FetchExecutor(0, vkLib::QueueAccessType::eGeneric);

	mConfig->mPipelineBuilder = ctx.MakePipelineBuilder();
	mConfig->mResourcePool = mConfig->mCtx.CreateResourcePool();
	mConfig->mRenderCtxFactory.SetContextBuilder(ctx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	mConfig->mFrontEnd.SetCtx(ctx);
	mConfig->mBackEnd.SetCtx(ctx);

	mConfig->mCmdAlloc = ctx.CreateCommandPools()[0];

	for (auto worker : mConfig->mWorkers)
	{
		mConfig->mCmdBufs.push_back(mConfig->mCmdAlloc.Allocate());
		mConfig->mRenderableCmds.push_back(mConfig->mCmdAlloc.Allocate());
		mConfig->mCopyIndices.push_back(mConfig->mPipelineBuilder.BuildComputePipeline<CopyIdxPipeline>(mConfig->mCopyIdxShader));
		mConfig->mCPUSignals.emplace_back(MakeRef<std::binary_semaphore>(1));
	}

	mConfig->mRenderGraphBuilder.SetCtx(ctx);
	mConfig->mRenderableBuilder.SetRscPool(mConfig->mResourcePool);

	SetupFeatureInfos();
	SetupVertexFactory();
	SetupHSTargetCtx();
	SetupHSVFactories();

	mConfig->mCamera = mConfig->mResourcePool.CreateBuffer<CameraInfo>(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mConfig->mFeatures = mConfig->mResourcePool.CreateBuffer<FeaturesEnabled>(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
	mConfig->mModels = mConfig->mResourcePool.CreateBuffer<glm::mat4>(vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mCamera.Resize(1);
	mConfig->mFeatures.Resize(1);

	vkLib::SamplerInfo depthSamplerInfo{};
	depthSamplerInfo.MagFilter = vk::Filter::eNearest;
	depthSamplerInfo.MinFilter = vk::Filter::eNearest;

	mConfig->mShadingSampler = mConfig->mResourcePool.CreateSampler({});
	mConfig->mDepthSampler = mConfig->mResourcePool.CreateSampler(depthSamplerInfo);

	mConfig->mMaterialSystem.SetCtx(ctx);
	mConfig->mMaterialSystem.SetHyperSurfaceRenderFactory(mConfig->mHSTargetCtx);
}

void AQUA_NAMESPACE::Renderer::EnableFeatures(RendererFeatureFlags flags)
{
	mConfig->mFeatureFlags |= flags;
	SetFeatures(flags, mConfig->mFeatures, true);
	mConfig->mFrontEnd.SetFeatureFlags(mConfig->mFeatureFlags);
	mConfig->mBackEnd.SetFeatureFlags(mConfig->mFeatureFlags);
}

void AQUA_NAMESPACE::Renderer::DisableFeatures(RendererFeatureFlags flags)
{
	mConfig->mFeatureFlags &= RendererFeatureFlags(~(uint64_t)flags);
	SetFeatures(flags, mConfig->mFeatures, false);
	mConfig->mFrontEnd.SetFeatureFlags(mConfig->mFeatureFlags);
	mConfig->mBackEnd.SetFeatureFlags(mConfig->mFeatureFlags);
}

void AQUA_NAMESPACE::Renderer::SetShadingbuffer(vkLib::Framebuffer framebuffer)
{
	mConfig->mShadingbuffer = framebuffer;
}

void AQUA_NAMESPACE::Renderer::SetSSAOConfig(const SSAOFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].UniBuffer.SetBuf(&config, &config + 1, 0);
}

void AQUA_NAMESPACE::Renderer::SetShadowConfig(const ShadowCascadeFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eShadow].UniBuffer.SetBuf(&config, &config + 1, 0);
	mConfig->mFrontEnd.SetShadowFeature(config);
}

void AQUA_NAMESPACE::Renderer::SetBloomEffectConfig(const BloomEffectFeature& config)
{
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].UniBuffer.SetBuf(&config, &config + 1);
}

void AQUA_NAMESPACE::Renderer::SetEnvironment(EnvironmentRef env)
{
	// Making sure that the environment has buffers
	// TODO: may break if we're dealing data across multiple queue families...
	env->RegenerateBuffers(mConfig->mResourcePool);
	mConfig->mEnv = env;

	mConfig->mFrontEnd.SetEnvironment(env);
	mConfig->mBackEnd.SetEnvironment(env);
}

void AQUA_NAMESPACE::Renderer::SetCamera(const glm::mat4& projection, const glm::mat4& view)
{
	CameraInfo camera{ projection, view };
	mConfig->mCamera.SetBuf(&camera, &camera + 1);
}

void AQUA_NAMESPACE::Renderer::PrepareFeatures()
{
	mConfig->mEnv->Update();

	mConfig->mFrontEnd.PrepareFramebuffers(mConfig->mShadingbuffer.GetResolution());
	mConfig->mBackEnd.PrepareFramebuffers(mConfig->mShadingbuffer.GetResolution());
	mConfig->mBackEnd.SetShadingFrameBuffer(mConfig->mShadingbuffer);

	mConfig->mFrontEnd.PrepareFeatures();
	mConfig->mBackEnd.PrepareFeatures();
}

void AQUA_NAMESPACE::Renderer::InsertPreEventDependency(vkLib::Core::Ref<vk::Semaphore> signal)
{
	auto graphList = mConfig->mFrontEnd.GetGraphList();
	auto graph = mConfig->mFrontEnd.GetGraph();

	if (graphList.empty())
		return;

	EXEC_NAMESPACE::DependencyInjection inInj{};
	inInj.Connect(graphList.front()->Name);
	inInj.SetSignal(signal);
	inInj.SetWaitPoint({});

	_STL_VERIFY(graph.InjectInputDependencies(inInj), "couldn't inject dependency");
}

void AQUA_NAMESPACE::Renderer::InsertPostEventDependency(vkLib::Core::Ref<vk::Semaphore> signal, vk::PipelineStageFlags pipelineFlags /*= vk::PipelineStageFlagBits::eTopOfPipe*/)
{
	auto graphList = mConfig->mBackEnd.GetGraphList();
	auto graph = mConfig->mBackEnd.GetGraph();

	if (graphList.empty())
		return;

	EXEC_NAMESPACE::DependencyInjection outInj{};
	outInj.Connect(graphList.back()->Name);
	outInj.SetSignal(signal);
	outInj.SetWaitPoint({});

	_STL_VERIFY(graph.InjectOutputDependencies(outInj), "couldn't inject dependency");
}

void AQUA_NAMESPACE::Renderer::SubmitRenderable(const std::string& name, const glm::mat4& model, Renderable renderable, MaterialInstance instance)
{
	auto config = mConfig;

	mConfig->mRenderables[name] = renderable;

	auto found = FindMaterialInstance(instance, config);

	/****************** Copying the model idx, material idx and the parameter set info *********************/

	VertexMetaData& vertexData = mConfig->mRenderables[name].VertexData;
	vertexData.ModelIdx = static_cast<uint32_t>(mConfig->mModels.GetSize());
	vertexData.MaterialIdx = found == mConfig->mMaterials.end() ?
		static_cast<uint32_t>(mConfig->mMaterials.size()) : static_cast<uint32_t>(found - mConfig->mMaterials.begin());
	vertexData.ParameterOffset = static_cast<uint32_t>(instance.GetOffset());

	vertexData.Stride = sizeof(glm::vec3);
	vertexData.Offset = 0;

	auto vertexDataBuffer = mConfig->mResourcePool.CreateGenericBuffer(renderable.Info.Usage, vk::MemoryPropertyFlagBits::eHostCoherent);

	Renderable::UpdateMetaData(vertexDataBuffer, vertexData, 0, static_cast<uint32_t>(renderable.Info.Mesh.aPositions.size()));

	mConfig->mVertexMetaBuffers[name] = vertexDataBuffer;

	InsertModelMatrix(model);

	auto materialIdx = static_cast<uint32_t>(config->mMaterials.size());
	
	if (found == mConfig->mMaterials.end())
		InsertMaterial(instance, [this, materialIdx](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
			{
				ExecuteMaterial(buffer, op, materialIdx);
			}, [config, instance](const EXEC_NAMESPACE::Operation& op)
				{
					op.GFX->SetClearDepthStencilValues(1.0f, 0);
					instance.UpdateDescriptors();
				});

	/************************************************************************************************/
}

void AQUA_NAMESPACE::Renderer::SubmitRenderable(const std::string& name, const glm::mat4& model, const MeshData& mesh, MaterialInstance instance)
{
	RenderableInfo info{};
	info.Mesh = mesh;

	auto renderable = mConfig->mRenderableBuilder.CreateRenderable(info);
	SubmitRenderable(name, model, renderable, instance);
}

void AQUA_NAMESPACE::Renderer::SubmitLines(const std::string& lineIsland, const vk::ArrayProxy<Line>& lines)
{
	auto config = mConfig;

	config->mHSLinesVertices[lineIsland] = CreateHSVBuffer();
	config->mHSLinesVertices[lineIsland].Resize(4 * lines.size() * sizeof(glm::vec4));

	auto memory = config->mHSLinesVertices[lineIsland].MapMemory<glm::vec4>(4 * lines.size());
	auto endMemory = memory + 4 * lines.size();

	std::for_each(lines.begin(), lines.end(), [&memory](const Line& line)
		{
			memory[0] = line.Begin.Position;
			memory[1] = line.Begin.Color;

			memory[2] = line.End.Position;
			memory[3] = line.End.Color;

			memory += 4;
		});

	config->mHSLinesVertices[lineIsland].UnmapMemory();

	MaterialInstance lineMaterial = *mConfig->mMaterialSystem[TEMPLATE_LINE];

	auto found = FindMaterialInstance(lineMaterial, config);

	if (found == mConfig->mMaterials.end())
		InsertMaterial(lineMaterial, [this](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
			{
				ExecuteLineMaterial(op, buffer);
			}, [this](const EXEC_NAMESPACE::Operation& op)
				{
					UpdateLineMaterial(op);
				});
}

void AQUA_NAMESPACE::Renderer::SubmitCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& connections)
{
	auto config = mConfig;

	uint32_t TotalPoints = 0;

	for (const auto& points : connections)
	{
		TotalPoints += static_cast<uint32_t>(points.Points.size());
	}

	config->mHSLinesVertices[curveIsland] = CreateHSVBuffer();
	config->mHSLinesVertices[curveIsland].Resize(4 * TotalPoints * sizeof(glm::vec4));

	TotalPoints = 0;

	for (const auto& points : connections)
	{
		auto memory = config->mHSLinesVertices[curveIsland].MapMemory<glm::vec4>(
			4 * static_cast<uint32_t>(points.Points.size()), 4 * TotalPoints);

		auto endMemory = memory + 4 * static_cast<uint32_t>(points.Points.size());

		TotalPoints += static_cast<uint32_t>(points.Points.size());

		for (uint32_t i = 0; i < points.Points.size() - 1; i++)
		{
			const auto& curr = points.Points.data()[i];
			const auto& next = points.Points.data()[i + 1];

			memory[0] = curr;
			memory[1] = points.Color;

			memory[2] = next;
			memory[3] = points.Color;

			memory += 4;
		}
	
		config->mHSLinesVertices[curveIsland].UnmapMemory();
	}

	MaterialInstance lineMaterial = *mConfig->mMaterialSystem[TEMPLATE_LINE];

	auto found = FindMaterialInstance(lineMaterial, config);

	if (found == mConfig->mMaterials.end())
		InsertMaterial(lineMaterial, [this](vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op)
			{
				ExecuteLineMaterial(op, buffer);
			}, [this](const EXEC_NAMESPACE::Operation& op)
				{
					UpdateLineMaterial(op);
				});
}

void AQUA_NAMESPACE::Renderer::SubmitBezierCurves(const std::string& curveIsland, const vk::ArrayProxy<Curve>& curves)
{
	std::vector<Curve> basicCurves;
	basicCurves.reserve(curves.size());

	for (const auto& curve : curves)
	{
		BezierCurve curveSolver(curve);
		auto& thisCurve = basicCurves.emplace_back();

		thisCurve.Points = curveSolver.Solve();
		thisCurve.Color = curve.Color;
		thisCurve.Thickness = curve.Thickness;
	}

	SubmitCurves(curveIsland, basicCurves);
}

void AQUA_NAMESPACE::Renderer::RemoveRenderable(const std::string& name)
{
	mConfig->mRenderables.erase(name);
	mConfig->mVertexMetaBuffers.erase(name);

	if (std::find(mConfig->mActiveRenderables.begin(), mConfig->mActiveRenderables.end(), name) == mConfig->mActiveRenderables.end())
		mConfig->mActiveRenderables.erase(name);
}

void AQUA_NAMESPACE::Renderer::ClearRenderables()
{
	// Resetting every state relating GPU shading network memory
	mConfig->mRenderables.clear();
	mConfig->mMaterials.clear();
	mConfig->mActiveRenderables.clear();
	mConfig->mVertexMetaBuffers.clear();
	mConfig->mModels.Clear();
}

void AQUA_NAMESPACE::Renderer::PrepareMaterialNetwork()
{
	mConfig->mFrontEnd.SetModels(mConfig->mModels);
	mConfig->mBackEnd.SetModels(mConfig->mModels);

	PrepareFramebuffers();
	PrepareShadingNetwork();

	mConfig->mShadingNetwork = *mConfig->mRenderGraphBuilder.GenerateExecutionGraph({ "SkyboxStage" });

	UpdateMaterialData();
	mConfig->mFrontEnd.UpdateGraph();
	mConfig->mBackEnd.UpdateGraph();

	mConfig->mShadingNetworkGraphList = mConfig->mShadingNetwork.SortEntries();

	ConnectFrontEndToShadingNetwork();
	ConnectBackEndToShadingNetwork();

	auto FrontEndGraphList = mConfig->mFrontEnd.GetGraphList();
	auto BackEndGraphList = mConfig->mBackEnd.GetGraphList();

	uint32_t nodeCount = static_cast<uint32_t>(mConfig->mShadingNetworkGraphList.size() + FrontEndGraphList.size() + BackEndGraphList.size());
	ResizeCmdBufferPool(nodeCount);
}

AQUA_NAMESPACE::SurfaceType AQUA_NAMESPACE::Renderer::GetSurfaceType(const std::string& name)
{
	// i could define the function template, but i think we're fine with if statements
	if (mConfig->mRenderables.find(name) != mConfig->mRenderables.end())
		return SurfaceType::eModel;

	if (mConfig->mHSLinesVertices.find(name) != mConfig->mHSLinesVertices.end())
		return SurfaceType::eLine;

	if (mConfig->mHSPointsVertices.find(name) != mConfig->mHSPointsVertices.end())
		return SurfaceType::ePoint;

	return SurfaceType::eNone;
}

void AQUA_NAMESPACE::Renderer::ActivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	for (const auto& name : names)
	{
		SurfaceType surfaceType = GetSurfaceType(name);

		switch (surfaceType)
		{
			case SurfaceType::eModel:
				mConfig->mActiveRenderables.insert(name);
				break;
			case SurfaceType::eLine:
				mConfig->mActiveLines.insert(name);
				break;
			case SurfaceType::ePoint:
				mConfig->mActivePoints.insert(name);
				break;
			default:
				break;
		}
	}
}

void AQUA_NAMESPACE::Renderer::DeactivateRenderables(const vk::ArrayProxy<std::string>& names)
{
	for (const auto& name : names)
	{
		SurfaceType surfaceType = GetSurfaceType(name);

		switch (surfaceType)
		{
		case SurfaceType::eModel:
			mConfig->mActiveRenderables.erase(name);
			break;
		case SurfaceType::eLine:
			mConfig->mActiveLines.erase(name);
			break;
		case SurfaceType::ePoint:
			mConfig->mActivePoints.erase(name);
			break;
		default:
			break;
		}
	}
}

void AQUA_NAMESPACE::Renderer::UploadRenderables()
{
	UploadModels();
	UploadLines();
	UploadPoints();
}

void AQUA_NAMESPACE::Renderer::UpdateDescriptors()
{
	mConfig->mShadingNetwork.Update();
}

void AQUA_NAMESPACE::Renderer::IssueDrawCall()
{
	// need a way to sync with the upload renderables routine
	size_t i = 0;
	uint32_t cmdIdx = 0;

	auto FrontEndGraphList = mConfig->mFrontEnd.GetGraphList();
	auto BackEndGraphList = mConfig->mBackEnd.GetGraphList();

	for (auto frontNodeRef : FrontEndGraphList)
	{
		auto& frontEndNode = *frontNodeRef;
		frontEndNode(mConfig->mCmdBufs[cmdIdx++], mConfig->mWorkers);
	}

	for (auto shadingNodeRef : mConfig->mShadingNetworkGraphList)
	{
		auto& shadingNode = *shadingNodeRef;
		shadingNode(mConfig->mCmdBufs[cmdIdx++], mConfig->mWorkers);
	}

	for (auto backNodeRef : BackEndGraphList)
	{
		auto& backEndNode = *backNodeRef;
		backEndNode(mConfig->mCmdBufs[cmdIdx++], mConfig->mWorkers);
	}
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetPostprocessbuffer() const
{
	return mConfig->mBackEnd.GetPostprocessbuffer();
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetShadingbuffer() const
{
	return mConfig->mShadingbuffer;
}

AQUA_NAMESPACE::VertexBindingMap AQUA_NAMESPACE::Renderer::GetVertexBindings() const
{
	return mConfig->mVertexBindingsInfo;
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetGBuffer() const
{
	return mConfig->mGBuffer;
}

vkLib::Framebuffer AQUA_NAMESPACE::Renderer::GetDepthbuffer() const
{
	return mConfig->mFrontEnd.GetDepthbuffers()[0];
}

vkLib::ImageView AQUA_NAMESPACE::Renderer::GetDepthView() const
{
	return mConfig->mFrontEnd.GetDepthViews()[0];
}

void AQUA_NAMESPACE::Renderer::InsertModelMatrix(const glm::mat4& model)
{
	// double the capacity if we hit the buffer limit
	if (mConfig->mModels.GetCapacity() < mConfig->mModels.GetSize() + 1)
		mConfig->mModels.Reserve(2 * mConfig->mModels.GetSize());

	mConfig->mModels << model;
}

void AQUA_NAMESPACE::Renderer::InsertMaterial(MaterialInstance& instance, 
	EXEC_NAMESPACE::OpFn&& opFn, EXEC_NAMESPACE::OpUpdateFn&& updateFn)
{
	auto config = mConfig;

	RendererConfig::MaterialInfo info{};
	info.Info = instance.GetInfo();
	info.Op = instance.GetMaterial();

	// make a copy to the underlying graphics pipeline
	info.Op.GFX = MakeRef(*instance.GetMaterial().GFX);

	// holding one instance of the material instance here
	info.Op.UpdateFn = updateFn;
	info.Op.Fn = opFn;

	config->mMaterials.push_back(info);
}

void AQUA_NAMESPACE::Renderer::UpdateLineMaterial(const EXEC_NAMESPACE::Operation& op)
{
	auto& hsLinePipeline = *op.GFX;

	auto resolution = mConfig->mShadingbuffer.GetResolution();

	vkLib::UniformBufferWriteInfo writeInfo{};
	writeInfo.Buffer = mConfig->mCamera.GetNativeHandles().Handle;

	hsLinePipeline.UpdateDescriptor({ 0, 0, 0 }, writeInfo);

	hsLinePipeline.SetVertexBuffer(0, mConfig->mHSVLinesFactory[TEMPLATE_POINT]);

	hsLinePipeline.SetScissor(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(resolution.x, resolution.y)));
	hsLinePipeline.SetViewport(vk::Viewport(0.0f, 0.0f, (float)resolution.x,
		(float)resolution.y, 0.0f, 1.0f));
}

void AQUA_NAMESPACE::Renderer::UploadModels()
{
	// Will be called per frame so it's supposed to work super fast
	// We'll be utilizing GPU to fill the vertex buffers of the renderer
	uint32_t vertexCount = 0;
	uint32_t renderableIdx = 0;

	mConfig->mVertexFactory.ClearBuffers();

	// first reserve the buffer space, and then transfer the data
	ReserveVertexFactorySpace(CalculateActiveVertexCount(), CalculateActiveIndexCount());

	for (const auto& renderableName : mConfig->mActiveRenderables)
	{
		uint32_t freeQueue = mConfig->mWorkers.FreeQueue(std::chrono::nanoseconds(0));

		auto renderable = mConfig->mRenderables[renderableName];
		auto vertexMetaBuf = mConfig->mVertexMetaBuffers[renderableName];
		auto cmd = mConfig->mRenderableCmds[freeQueue];

		cmd.reset();
		cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		auto& buffers = renderable.mVertexBuffers;

		mConfig->mVertexFactory.TraverseBuffers([&buffers, cmd](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
			{
				// vertex meta infos are copied separately
				if (name == ENTRY_METADATA)
					return;

				// requires a one to one correspondence b/w vertex factory names and the renderable ref vertex buffer names
				// may a mismatch occurs, we'll have a runtime exception at the 'at' method
				VertexFactory::CopyVertexBuffer(cmd, buffer, buffers.at(name));
			});

		// vertex meta infos are copied separately
		VertexFactory::CopyVertexBuffer(cmd, mConfig->mVertexFactory[ENTRY_METADATA], vertexMetaBuf);

		mConfig->mCopyIndices[freeQueue](cmd, mConfig->mVertexFactory.GetIndexBuffer(), renderable.mIndexBuffer, vertexCount);
		vertexCount += static_cast<uint32_t>(renderable.Info.Mesh.GetVertexCount());

		cmd.end();

		// TODO: queue selection should occur inside the executor
		// TODO: Some queues might be busy in other threads, so we'll only wait for those who were utilized...
		mConfig->mWorkers.SubmitWork(cmd);
	}
}

void AQUA_NAMESPACE::Renderer::UploadLines()
{
	// Will be called per frame so it's supposed to work super fast
	// We'll be utilizing GPU to fill the vertex buffers of the renderer
	uint32_t vertexCount = 0;
	uint32_t renderableIdx = 0;

	mConfig->mHSVLinesFactory.ClearBuffers();

	// first reserve the buffer space, and then transfer the data
	mConfig->mHSVLinesFactory.ReserveVertices(CalculateActiveHSLVertexCount());

	for (const auto& renderableName : mConfig->mActiveLines)
	{
		uint32_t freeQueue = mConfig->mWorkers.FreeQueue(std::chrono::nanoseconds(0));

		auto lineBuffer = mConfig->mHSLinesVertices[renderableName];
		auto cmd = mConfig->mRenderableCmds[freeQueue];

		cmd.reset();
		cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		mConfig->mHSVLinesFactory.TraverseBuffers([&lineBuffer, cmd](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
			{
				// requires a one to one correspondence b/w vertex factory names and the renderable ref vertex buffer names
				// may a mismatch occurs, we'll have a runtime exception at the 'at' method
				VertexFactory::CopyVertexBuffer(cmd, buffer, lineBuffer);
			});

		cmd.end();

		// TODO: queue selection should occur inside the executor
		// TODO: Some queues might be busy in other threads, so we'll only wait for those who were utilized...
		mConfig->mWorkers[freeQueue]->Submit(cmd);
	}
}

void AQUA_NAMESPACE::Renderer::UploadPoints()
{
	// Will be called per frame so it's supposed to work super fast
// We'll be utilizing GPU to fill the vertex buffers of the renderer
	uint32_t vertexCount = 0;
	uint32_t renderableIdx = 0;

	mConfig->mHSVPointsFactory.ClearBuffers();

	// first reserve the buffer space, and then transfer the data
	mConfig->mHSVPointsFactory.ReserveVertices(CalculateActiveHSPVertexCount());

	for (const auto& renderableName : mConfig->mActivePoints)
	{
		uint32_t freeQueue = mConfig->mWorkers.FreeQueue(std::chrono::nanoseconds(0));

		auto lineBuffer = mConfig->mHSPointsVertices[renderableName];
		auto cmd = mConfig->mRenderableCmds[freeQueue];

		cmd.reset();
		cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		mConfig->mHSVLinesFactory.TraverseBuffers([&lineBuffer, cmd](uint32_t idx, 
			const std::string& name, vkLib::GenericBuffer buffer)
			{
				// requires a one to one correspondence b/w vertex factory names and the renderable ref vertex buffer names
				// may a mismatch occurs, we'll have a runtime exception at the 'at' method
				VertexFactory::CopyVertexBuffer(cmd, buffer, lineBuffer);
			});

		cmd.end();

		// TODO: queue selection should occur inside the executor
		// TODO: Some queues might be busy in other threads, so we'll only wait for those who were utilized...
		mConfig->mWorkers[freeQueue]->Submit(cmd);
	}
}

void AQUA_NAMESPACE::Renderer::PrepareShadingNetwork()
{
	// TODO: reading the required geometry resources from the materials
	// and producing the corresponding g buffers and mapping them to correct lighting passes

	auto config = mConfig;

	mConfig->mRenderGraphBuilder.Clear();

	// todo: a bit inefficient since the geometry is relatively fixed for each material for now
	GBufferPlugin gbuffer{};
	gbuffer.SetBindings(config->mVertexBindingsInfo); // This data can be read from the material instance
	gbuffer.SetPipelineBuilder(config->mPipelineBuilder);
	gbuffer.SetGBuffer(mConfig->mGBuffer);
	gbuffer.SetShader(mConfig->mGBufferShader);
	gbuffer.AddPlugin(mConfig->mRenderGraphBuilder, "GBufferStage");

	mConfig->mRenderGraphBuilder["GBufferStage"].OpID = RendererConfig::sGBufferID;
	mConfig->mRenderGraphBuilder["GBufferStage"].UpdateFn = [config](EXEC_NAMESPACE::Operation& op)
		{
			auto& pipeline = *reinterpret_cast<DeferredPipeline*>(GetRefAddr(op.GFX));

			pipeline.SetClearDepthStencilValues(1.0f, 0);

			pipeline.SetClearColorValues(0, { 1.0f, 0.0f, 1.0f, 1.0f });

			// fetching data from the vertex factory; can only be done once the
			pipeline.SetVertexBuffer(0, config->mVertexFactory[ENTRY_POSITION]);
			pipeline.SetVertexBuffer(1, config->mVertexFactory[ENTRY_NORMAL]);
			pipeline.SetVertexBuffer(2, config->mVertexFactory[ENTRY_TANGENT_SPACE]);
			pipeline.SetVertexBuffer(3, config->mVertexFactory[ENTRY_TEXCOORDS]);
			pipeline.SetVertexBuffer(4, config->mVertexFactory[ENTRY_METADATA]);

			pipeline.SetIndexBuffer(config->mVertexFactory.GetIndexBuffer());

			pipeline.SetCamera(config->mCamera);
			pipeline.SetModelMatrices(config->mModels);

			pipeline.UpdateDescriptors();
		};

	std::string materialPrefix = "DeferMat_";
	uint32_t materialIdx = 0;

	for (const auto& rawMaterial : mConfig->mMaterials)
	{
		std::string instanceName = materialPrefix + std::to_string(materialIdx++);
		mConfig->mRenderGraphBuilder[instanceName] = rawMaterial.Op;
		mConfig->mRenderGraphBuilder[instanceName].OpID = RendererConfig::sMatTypeID;

		mConfig->mRenderGraphBuilder.InsertDependency("GBufferStage", instanceName);
		mConfig->mRenderGraphBuilder.InsertDependency(instanceName, "SkyboxStage");
	}

	SkyboxPlugin skyboxPlugin{};
	skyboxPlugin.SetPipelineBuilder(mConfig->mPipelineBuilder);
	skyboxPlugin.SetFramebuffer(mConfig->mShadingbuffer);
	skyboxPlugin.SetPShader(mConfig->mSkyboxShader);
	skyboxPlugin.AddPlugin(mConfig->mRenderGraphBuilder, "SkyboxStage");

	mConfig->mRenderGraphBuilder["SkyboxStage"].UpdateFn = [config](EXEC_NAMESPACE::Operation& op)
		{
			auto& pipeline = *reinterpret_cast<SkyboxPipeline*>(op.GFX.get());

			pipeline.SetClearDepthStencilValues(1.0f, 0);
			
			pipeline.SetCamera(config->mCamera);
			pipeline.SetEnvironmentTexture(*config->mEnv->GetSkybox(), config->mEnv->GetSampler());

			pipeline.UpdateDescriptors();
		};
}

void AQUA_NAMESPACE::Renderer::PrepareFramebuffers()
{
	auto& rcFac = mConfig->mRenderCtxFactory;

	rcFac.Clear();

	rcFac.AddColorAttribute(ENTRY_POSITION, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_NORMAL, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_TEXCOORDS, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_TANGENT, "RGBA32F");
	rcFac.AddColorAttribute(ENTRY_BITANGENT, "RGBA32F");

	rcFac.SetDepthAttribute("Depth", "D24UN_S8U");

	rcFac.SetAllColorProperties(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	rcFac.SetDepthProperties(vk::ImageUsageFlagBits::eDepthStencilAttachment);

	auto error = rcFac.Validate();
	_STL_ASSERT(error, "Couldn't validate the render factory");

	rcFac.SetImageView("Depth", mConfig->mShadingbuffer.GetDepthStencilAttachment());

	mConfig->mGBuffer = *rcFac.CreateFramebuffer();
}

void AQUA_NAMESPACE::Renderer::ConnectFrontEndToShadingNetwork()
{
	// all the light src depth buffers will map to all the material pipeline
	// if there are m light src, n division in the cascade, and p materials
	// there will be m by n by p connections

	const auto& frontEndOutputs = mConfig->mFrontEnd.GetOutputs();

	for (const auto& output : frontEndOutputs)
	{
		for (const auto& [name, node] : mConfig->mShadingNetwork.Nodes)
		{
			if (node->OpID != RendererConfig::sMatTypeID)
				continue;
				
			EXEC_NAMESPACE::DependencyInjection outInj{};
			outInj.Connect(output);
			outInj.SetSignal(mConfig->mCtx.CreateSemaphore());
			outInj.SetWaitPoint(vk::PipelineStageFlagBits::eColorAttachmentOutput);

			auto FrontGraph = mConfig->mFrontEnd.GetGraph();
			auto error = FrontGraph.InjectOutputDependencies(outInj);

			_STL_ASSERT(error, "couldn't inject dependency");

			EXEC_NAMESPACE::DependencyInjection inInj{};
			inInj.Connect(node->Name);
			inInj.SetSignal(outInj.Signal);

			error = mConfig->mShadingNetwork.InjectInputDependencies(inInj);

			_STL_ASSERT(error, "couldn't inject dependency");
		}
	}
}

void AQUA_NAMESPACE::Renderer::ConnectBackEndToShadingNetwork()
{
	const auto& backEndOutputs = mConfig->mBackEnd.GetInputs();

	for (const auto& input : backEndOutputs)
	{
		EXEC_NAMESPACE::DependencyInjection inInj{};
		inInj.Connect(input);
		inInj.SetSignal(mConfig->mCtx.CreateSemaphore());
		inInj.SetWaitPoint(vk::PipelineStageFlagBits::eColorAttachmentOutput);

		auto BackGraph = mConfig->mBackEnd.GetGraph();

		auto error = BackGraph.InjectInputDependencies(inInj);

		_STL_ASSERT(error, "couldn't inject dependency");

		EXEC_NAMESPACE::DependencyInjection outInj{};
		outInj.Connect("SkyboxStage");
		outInj.SetSignal(inInj.Signal);
		error = mConfig->mShadingNetwork.InjectOutputDependencies(outInj);

		_STL_ASSERT(error, "couldn't inject dependency");
	}
}

void AQUA_NAMESPACE::Renderer::SetupFeatureInfos()
{
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Name = "ShadowStage";
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Stage = RenderingStage::eFrontEnd;
	mConfig->mFeatureInfos[RenderingFeature::eShadow].Type = RenderingFeature::eShadow;
	mConfig->mFeatureInfos[RenderingFeature::eShadow].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Name = "BloomEffectStage";
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Stage = RenderingStage::eBackEnd;
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].Type = RenderingFeature::eBloomEffect;
	mConfig->mFeatureInfos[RenderingFeature::eBloomEffect].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Name = "SSAOStage";
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Stage = RenderingStage::eBackEnd;
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].Type = RenderingFeature::eSSAO;
	mConfig->mFeatureInfos[RenderingFeature::eSSAO].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Name = "MotionBlurStage";
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Stage = RenderingStage::eFrontEnd;
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].Type = RenderingFeature::eMotionBlur;
	mConfig->mFeatureInfos[RenderingFeature::eMotionBlur].UniBuffer =
		mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);

	mConfig->mFrontEnd.SetFeaturesInfos(mConfig->mFeatureInfos);
	mConfig->mBackEnd.SetFeaturesInfos(mConfig->mFeatureInfos);
}

void AQUA_NAMESPACE::Renderer::SetupVertexBindings()
{
	auto& vertexBindings = mConfig->mVertexBindingsInfo;

	vertexBindings.clear();
	vertexBindings[0].SetName(ENTRY_POSITION);
	vertexBindings[0].AddAttribute(0, "RGB32F");

	vertexBindings[1].SetName(ENTRY_NORMAL);
	vertexBindings[1].AddAttribute(1, "RGB32F");

	vertexBindings[2].SetName(ENTRY_TANGENT_SPACE);
	vertexBindings[2].AddAttribute(2, "RGB32F");
	vertexBindings[2].AddAttribute(3, "RGB32F");

	vertexBindings[3].SetName(ENTRY_TEXCOORDS);
	vertexBindings[3].AddAttribute(4, "RGB32F");

	vertexBindings[4].SetName(ENTRY_METADATA);
	vertexBindings[4].AddAttribute(5, "RGB32F");
}

void AQUA_NAMESPACE::Renderer::SetupVertexFactory()
{
	auto& vertexFac = mConfig->mVertexFactory;

	vertexFac.ClearBuffers();
	vertexFac.Reset();
	
	vertexFac.SetResourcePool(mConfig->mResourcePool);
	vertexFac.SetVertexBindings(mConfig->mVertexBindingsInfo);
	vertexFac.SetAllVertexProperties(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);

	vertexFac.SetIndexProperties(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);

	auto error = vertexFac.Validate();
	_STL_ASSERT(error, "Couldn't setup the vertex factory in the renderer");

	vertexFac.ReserveVertices(mConfig->mReservedVertices);
	vertexFac.ReserveIndices(mConfig->mReservedIndices);

	mConfig->mFrontEnd.SetVertexFactory(vertexFac);
}

void AQUA_NAMESPACE::Renderer::UpdateMaterialData()
{
	auto& gBuffer = mConfig->mGBuffer;
	auto depthViews = mConfig->mFrontEnd.GetDepthViews();
	auto sampler = mConfig->mShadingSampler;
	const auto& lightBuffers = mConfig->mEnv->GetLightBuffers();

	for (const auto& material : mConfig->mMaterials)
	{
		auto& materialInfo = *material.Info;

		materialInfo.Resources[{0, 0, 0}].SetSampledImage(gBuffer.GetColorAttachments()[0], sampler);
		materialInfo.Resources[{0, 1, 0}].SetSampledImage(gBuffer.GetColorAttachments()[1], sampler);
		materialInfo.Resources[{0, 2, 0}].SetSampledImage(gBuffer.GetColorAttachments()[2], sampler);
		materialInfo.Resources[{0, 3, 0}].SetSampledImage(gBuffer.GetColorAttachments()[3], sampler);
		materialInfo.Resources[{0, 4, 0}].SetSampledImage(gBuffer.GetColorAttachments()[4], sampler);

		materialInfo.Resources[{0, 5, 0}].SetSampledImage(depthViews[0], mConfig->mDepthSampler);

		materialInfo.Resources[{1, 0, 0}].SetStorageBuffer(lightBuffers.mDirLightBuf.GetBufferChunk());
		materialInfo.Resources[{1, 1, 0}].SetStorageBuffer(lightBuffers.mPointLightBuf.GetBufferChunk());
		materialInfo.Resources[{1, 2, 0}].SetStorageBuffer(lightBuffers.mDirCameraInfos.GetBufferChunk());
		materialInfo.Resources[{1, 3, 0}].SetUniformBuffer(mConfig->mCamera.GetBufferChunk());
	}
}

void AQUA_NAMESPACE::Renderer::ExecuteMaterial(vk::CommandBuffer buffer, const EXEC_NAMESPACE::Operation& op, uint32_t materialIdx)
{
	EXEC_NAMESPACE::Executioner executioner(buffer, op);

	auto& pipeline = *op.GFX;
	auto& materialInfo = *mConfig->mMaterials[materialIdx].Info;

	pipeline.SetFramebuffer(mConfig->mShadingbuffer);

	pipeline.Begin(buffer);

	MaterialInstance::TraverseImageResources(materialInfo.Resources,
		[buffer](const vkLib::DescriptorLocation& descInfo, vkLib::ImageView& view)
		{
			view->BeginCommands(buffer);
			view->RecordTransitionLayout(vk::ImageLayout::eGeneral);
		});

	pipeline.SetShaderConstant("eFragment.ShaderConstants.Index_0", static_cast<uint32_t>(mConfig->mEnv->GetDirLightCount()));
	pipeline.SetShaderConstant("eFragment.ShaderConstants.Index_1", static_cast<uint32_t>(mConfig->mEnv->GetPointLightCount()));
	//pipeline.SetShaderConstant("eFragment.ShaderConstants.Index_2", static_cast<uint32_t>(materialIdx));

	pipeline.Activate();
	pipeline.DrawVertices(0, 0, 1, 6);

	MaterialInstance::TraverseImageResources(materialInfo.Resources,
		[buffer](const vkLib::DescriptorLocation& descInfo, vkLib::ImageView& view)
		{
			view->EndCommands();
		});

	pipeline.End();
}

void AQUA_NAMESPACE::Renderer::ExecuteLineMaterial(const EXEC_NAMESPACE::Operation& op, vk::CommandBuffer buffer)
{
	EXEC_NAMESPACE::Executioner executioner(buffer, op);

	auto& hsLinePipeline = *op.GFX;

	hsLinePipeline.SetFramebuffer(mConfig->mShadingbuffer);

	hsLinePipeline.Begin(buffer);
	hsLinePipeline.Activate();

	// we need a to inform this functions that not all vertices in the buffer represent lines
	hsLinePipeline.DrawVertices(0, 0, 1, static_cast<uint32_t>(
		mConfig->mHSVLinesFactory[TEMPLATE_POINT].GetSize() / (2 * sizeof(glm::vec4))));

	hsLinePipeline.End();
}

void AQUA_NAMESPACE::Renderer::ResizeCmdBufferPool(size_t newSize)
{
	size_t preSize = mConfig->mCmdBufs.size();

	// reduction
	if (preSize > newSize)
	{
		std::for_each(mConfig->mCmdBufs.begin() + newSize, mConfig->mCmdBufs.end(), [this](vk::CommandBuffer buffer)
			{
				mConfig->mCmdAlloc.Free(buffer);
			});
	}

	mConfig->mCmdBufs.resize(newSize);

	// creation
	if (preSize < newSize)
	{
		std::for_each(mConfig->mCmdBufs.begin() + preSize, mConfig->mCmdBufs.end(), [this](vk::CommandBuffer& buffer)
			{
				buffer = mConfig->mCmdAlloc.Allocate();
			});
	}
}

void AQUA_NAMESPACE::Renderer::SetupShaders()
{
	mConfig->mGBufferShader.SetFilepath("eVertex", mConfig->mShaderDirectory + "Defer.vert");
	mConfig->mGBufferShader.SetFilepath("eFragment", mConfig->mShaderDirectory + "Defer.frag");

	auto error = mConfig->mGBufferShader.CompileShaders();

	CompileErrorChecker checker(mConfig->mShaderDirectory + "../Logging/ShaderError.glsl");

	checker.GetError(error[0]);
	checker.GetError(error[1]);

	checker.AssertOnError(error);

	mConfig->mSkyboxShader.SetFilepath("eVertex", mConfig->mShaderDirectory + "Skybox.vert");
	mConfig->mSkyboxShader.SetFilepath("eFragment", mConfig->mShaderDirectory + "Skybox.frag");

	mConfig->mSkyboxShader.AddMacro("MATH_PI", std::to_string(glm::pi<float>()));
	error = mConfig->mSkyboxShader.CompileShaders();

	checker.GetError(error[0]);
	checker.GetError(error[1]);

	checker.AssertOnError(error);

	mConfig->mCopyIdxShader.SetFilepath("eCompute", mConfig->mShaderDirectory + "CopyIdx.comp");

	error = mConfig->mCopyIdxShader.CompileShaders();

	checker.GetError(error[0]);

	checker.AssertOnError(error);

}

void AQUA_NAMESPACE::Renderer::ReserveVertexFactorySpace(uint32_t vertexCount, uint32_t indexCount)
{
	mConfig->mVertexFactory.ReserveVertices(vertexCount);
	mConfig->mVertexFactory.ReserveIndices(indexCount);
}

uint32_t AQUA_NAMESPACE::Renderer::CalculateActiveVertexCount()
{
	uint32_t vertexCount = 0;

	for (const auto& name : mConfig->mActiveRenderables)
	{
		vertexCount += static_cast<uint32_t>(mConfig->mRenderables[name].Info.Mesh.GetVertexCount());
	}

	return vertexCount;
}

uint32_t AQUA_NAMESPACE::Renderer::CalculateActiveIndexCount()
{
	uint32_t indexCount = 0;

	for (const auto& name : mConfig->mActiveRenderables)
	{
		indexCount += static_cast<uint32_t>(mConfig->mRenderables[name].Info.Mesh.GetIndexCount());
	}

	return indexCount;
}

uint32_t AQUA_NAMESPACE::Renderer::CalculateActiveHSLVertexCount()
{
	uint32_t vertexCount = 0;

	for (const auto& name : mConfig->mActiveLines)
	{
		vertexCount += static_cast<uint32_t>(mConfig->mHSLinesVertices[name].GetSize() / (2 * sizeof(glm::vec4)));
	}

	return vertexCount;
}

uint32_t AQUA_NAMESPACE::Renderer::CalculateActiveHSPVertexCount()
{
	uint32_t vertexCount = 0;

	for (const auto& name : mConfig->mActivePoints)
	{
		vertexCount += static_cast<uint32_t>(mConfig->mHSPointsVertices[name].GetSize() / (2 * sizeof(glm::vec4)));
	}

	return vertexCount;
}

void AQUA_NAMESPACE::Renderer::SetupHSTargetCtx()
{
	auto& renderFac = mConfig->mHSTargetCtx;

	renderFac.SetContextBuilder(mConfig->mCtx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	renderFac.SetAllColorProperties(vk::AttachmentLoadOp::eLoad,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

	renderFac.SetDepthProperties(vk::AttachmentLoadOp::eLoad,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);

	renderFac.AddColorAttribute("Color", "RGBA32F");
	renderFac.SetDepthAttribute("Depth", "D24Un_S8U");

	_STL_VERIFY(renderFac.Validate(), "invalid render factory");
}

void AQUA_NAMESPACE::Renderer::SetupHSVFactories()
{
	auto& hsvlFac = mConfig->mHSVLinesFactory;
	hsvlFac.SetResourcePool(mConfig->mResourcePool);
	
	VertexBindingMap bindings{};
	bindings[0].SetName(TEMPLATE_POINT);
	bindings[0].AddAttribute(0, "RGBA32F");
	bindings[0].AddAttribute(1, "RGBA32F");
	
	hsvlFac.SetVertexBindings(bindings);
	
	hsvlFac.SetAllVertexProperties(vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);
	
	_STL_VERIFY(hsvlFac.Validate(), "invalid vertex factory");
	
	hsvlFac.ReserveVertices(mConfig->mReservedVertices);
	hsvlFac.ReserveIndices(mConfig->mReservedIndices);

	auto& hsvpFac = mConfig->mHSVLinesFactory;
	hsvpFac.SetResourcePool(mConfig->mResourcePool);

	bindings.clear();
	bindings[0].SetName(TEMPLATE_POINT);
	bindings[0].AddAttribute(0, "RGBA32F");
	bindings[0].AddAttribute(1, "RGBA32F");

	hsvpFac.SetVertexBindings(bindings);

	hsvpFac.SetAllVertexProperties(vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostCoherent);

	_STL_VERIFY(hsvpFac.Validate(), "invalid vertex factory");

	hsvpFac.ReserveVertices(mConfig->mReservedVertices);
	hsvpFac.ReserveIndices(mConfig->mReservedIndices);
}

vkLib::GenericBuffer AQUA_NAMESPACE::Renderer::CreateHSVBuffer()
{
	return mConfig->mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eVertexBuffer
		| vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent);
}
